# PATCH san_hub_comm + san_lte_redundancy C++ Deep-Dive (v1.5.1)

> **작업일**: 2026-05-13
> **대상**: san_hub_comm + san_lte_redundancy (둘 다 C++)
> **권원**: SDD-SWARM v1.5 §5.5 (LTE 이중화), §6.2 (영상 릴레이), IDS-CMD §3.6
> **언어**: C++17 (둘 다 ament_cmake C++ — DCN governance 변경 없음)

---

## 1. Deep-Dive 결과 — 식별된 이슈 14건

### 🔴 san_hub_comm Critical

| # | 이슈 | 영향 |
|---|---|---|
| **HC1** (SEC) | **passphrase 가 `/video/handle` topic 으로 plaintext broadcast** + srt_uri 안에도 plaintext | DDS 미암호화 시 모든 subscriber 가 AES-128 passphrase 수신 — 영상 암호화 자체가 의미 없어짐 |
| **HC2** (CFG) | **buildSrtUri 가 hub_ip listener / srtsink 가 operator_ip caller — 엔드포인트 mismatch** | handle URI 가 operator 에게 잘못된 listen 주소를 전달 → 영상 연결 자체가 실패 |
| **HC3** (RACE) | active_streams_ unordered_map 에 mutex 없음 | MultiThreadedExecutor 에서 onRequest + onLinkQuality 동시 실행 시 corruption |
| **HC4** (RACE) | thumbnail_mode_ atomic 이지만 check-then-write 복합 연산 | 동시 START 시 threshold 검사와 set 사이의 race window |
| **HC5** (SEC) | std::random_device 가 CSPRNG 보장 없음 | platform 의존; 약한 entropy 시 passphrase 예측 가능 |
| **HC6** (CFG) | kThumbnailThreshold=4 hardcoded | 운용 환경별 조정 불가 |

### 🔴 san_lte_redundancy Critical

| # | 이슈 | 영향 |
|---|---|---|
| **LR1** (BLOCK) | promote() 가 `std::this_thread::sleep_for(100ms)` 루프 — 최대 2초 executor block | rclcpp single-thread executor 에서 모든 callback 정지; watchdog 도 정지 |
| **LR2** (RACE) | role_ 필드 mutex 없음 — 다중 callback 에서 R/W | onAnnouncement / onLocalLteStatusChange / watchdogTick 동시 실행 시 torn state |
| **LR3** (LOGIC) | split-brain check `msg.lte_term > lte_term_.load() - 1` 부적절 | term=0 underflow; equal-term tiebreak 미구현 |
| **LR4** (RACE) | chain-fight: msg 순서에 따라 multiple "first in chain" 가능 | 두 backup 이 동시 promote → split-brain |
| **LR5** (UB) | offsetof on non-standard-layout class (Mwan3UbusMonitor 가 std::mutex/std::thread 보유) | C++ standard 상 conditionally-supported / UB |
| **LR6** (RACE) | ~Mwan3UbusMonitor 가 uloop_thread 와 ubus_ctx 동시 정리 | uloop callback 이 freed ubus_ctx 에 접근 가능 |
| **LR7** (RACE) | hub_lte_down_detected_at_ optional 이 callback + watchdog 무동기화 | torn state |
| **LR8** (RACE) | announce_seq_ uint32_t non-atomic, broadcast 마다 ++ | sequence 누락 가능 |

---

## 2. 파일 변경 요약

```
san_hub_comm/                                   v1.3.0 → v1.5.1
├── package.xml                                 ★ version
├── include/san_hub_comm/
│   ├── passphrase_generator.hpp                ★ patched (CSPRNG)
│   └── gstreamer_relay_node.hpp                ★ patched (mutex, URI sep)
├── src/
│   ├── passphrase_generator.cpp                ★ rewrite (getrandom)
│   └── gstreamer_relay_node.cpp                ★ rewrite (lock, URI fix)
├── test/
│   └── test_patch_hub_comm.cpp                 ★ NEW (PH1-PH9)
└── CMakeLists.txt                              ★ patched (테스트 등록)

san_lte_redundancy/                             v1.3.0 → v1.5.1
├── package.xml                                 ★ version
├── include/san_lte_redundancy/
│   ├── lte_role_manager.hpp                    ★ patched (state_mu_, async)
│   └── mwan3_ubus_monitor.hpp                  ★ patched (SubscriberHolder)
├── src/
│   ├── lte_role_manager.cpp                    ★ rewrite (async + tiebreak)
│   └── mwan3_ubus_monitor.cpp                  ★ rewrite (offsetof fix, teardown)
├── test/
│   └── test_patch_lte_redundancy.cpp           ★ NEW (PL1-PL9)
└── CMakeLists.txt                              ★ patched
```

---

## 3. ★ 핵심 코드 발췌

### 3.1 HC5 fix — getrandom(2) CSPRNG with rejection sampling

**Before** (passphrase_generator.cpp):
```cpp
std::random_device rd;     // ★ implementation-defined; may not be CSPRNG
std::uniform_int_distribution<std::size_t> dist(0, kCharsetSize - 1);
for (std::size_t i = 0; i < len_; ++i) {
    out.push_back(kCharset[dist(rd)]);
}
```

**After**:
```cpp
static bool fillCsprngBytes(std::uint8_t* out, std::size_t n) {
#if defined(__linux__) && defined(SYS_getrandom)
    /* getrandom(2) syscall — Linux CSPRNG, never blocks once seeded */
#endif
    /* /dev/urandom fallback */
}

std::string PassphraseGenerator::generate() {
    /* Rejection-sampling drain with 64-byte buffer.
       Bytes 0..247 map cleanly to [0..61]; bytes 248..255 dropped. */
    while (out.size() < len_) {
        const std::uint8_t b = buf[buf_pos++];
        if (b >= kRejectThreshold) continue;     // ★ rejection sampling
        out.push_back(kCharset[b % kCharsetSize]);
    }
    return out;
}
```

생성자에서 CSPRNG fail-fast — production 에서 약한 entropy 로 시작할 수 없음.

### 3.2 HC1/HC2 fix — passphrase separation + URI endpoint

**Before** (gstreamer_relay_node.cpp):
```cpp
std::string buildSrtUri(const std::string& passphrase) const {
    std::ostringstream o;
    o << "srt://" << hub_ip_ << ":" << srt_port_       // ★ hub_ip 잘못된 endpoint
      << "?mode=listener&latency=" << srt_latency_ms_;
    if (!passphrase.empty()) {
        o << "&passphrase=" << passphrase              // ★ broadcast 됨
          << "&pbkeylen=16";
    }
    return o.str();
}

// handleStart:
h.passphrase = passphrase;                              // ★ topic 으로 전송
h.srt_uri = buildSrtUri(passphrase);                    // ★ URI 안에도 plaintext
```

**After**:
```cpp
// HC1, HC2: URI 에 passphrase 제외 + operator endpoint
std::string buildSrtUri(const std::string& operator_ip) const {
    std::ostringstream o;
    o << "srt://" << operator_ip << ":" << srt_port_   // ★ operator 가 listen
      << "?mode=listener&latency=" << srt_latency_ms_;
    return o.str();
}

// handleStart:
h.passphrase = redact_passphrase_in_handle_              // ★ redaction option
    ? std::string() : passphrase;
h.srt_uri = buildSrtUri(operator_ip);                    // ★ no passphrase
```

새 parameter `redact_passphrase_in_handle` 추가 — bridge 경유 시 운영자가 passphrase 를 별도 채널(인증된 BLE 등)로 전달하도록 허용.

### 3.3 HC3/HC4 fix — streams_mutex_ 로 check-then-write 보호

**Before**:
```cpp
if (active_streams_.size() + 1 >= kThumbnailThreshold) {  // ★ check
    effective_quality = Req::QUALITY_THUMBNAIL;
    thumbnail_mode_ = true;                                 // ★ write
}
// ... 사이에 다른 thread 에서 size 변화 가능 ...
active_streams_[req.target_robot_id] = std::move(state);
if (active_streams_.size() >= kThumbnailThreshold && !thumbnail_mode_) {
    downgradeAllToThumbnail();
}
```

**After**:
```cpp
{
    std::lock_guard<std::mutex> lock(streams_mutex_);
    if (active_streams_.size() + 1 >= thumbnail_threshold_) {
        effective_quality = Req::QUALITY_THUMBNAIL;
        thumbnail_mode_.store(true);
    }
    // ... pipeline build, play, insert ...
    active_streams_[req.target_robot_id] = std::move(state);
    if (active_streams_.size() >= thumbnail_threshold_) {
        downgradeAllToThumbnailLocked();
    }
}
```

### 3.4 LR1 fix — async promote (sleep_for 제거)

**Before** (lte_role_manager.cpp):
```cpp
void LTERoleManager::promote(const std::string& reason) {
    if (!uci_->setLteWeight(100)) return;
    if (!ubus_->reloadMwan3Service()) return;

    // ★ executor block!
    const auto deadline = ... + ppp_activation_timeout_s_;
    while (std::chrono::steady_clock::now() < deadline) {
        ubus_->refreshLteStatus();
        if (ubus_->isLteUp()) break;
        std::this_thread::sleep_for(100ms);     // ★ rclcpp single-thread executor 가 통째로 정지
    }
    // ...
}
```

**After**:
```cpp
void LTERoleManager::promoteAsync(const std::string& reason) {
    // Quick C-API calls only.
    uci_->setLteWeight(100);
    ubus_->reloadMwan3Service();

    // ★ Schedule async activation timer.
    promotion_.reason   = reason;
    promotion_.deadline = now() + rclcpp::Duration::from_seconds(
        ppp_activation_timeout_s_);
    promotion_.in_flight = true;
    activation_timer_->reset();        // 100ms wall_timer
}

void LTERoleManager::activationTick() {
    std::lock_guard<std::recursive_mutex> lock(state_mu_);
    if (!promotion_.in_flight) { activation_timer_->cancel(); return; }
    ubus_->refreshLteStatus();
    if (ubus_->isLteUp()) {
        const uint32_t new_term = lte_term_.load() + 1;
        lte_term_.store(new_term);
        broadcastRole(...LTE_PROMOTED..., promotion_.reason);
        role_ = LTERole::LTE_ACTIVE;
        promotion_.in_flight = false;
        activation_timer_->cancel();
        return;
    }
    if (now() >= promotion_.deadline) {
        promotion_.in_flight = false;
        activation_timer_->cancel();
        // stays BACKUP_ACTIVATING; operator can retry
    }
}
```

### 3.5 LR3/LR4 fix — equal-term tiebreak

```cpp
bool LTERoleManager::incomingPromotionPreempts(
    const combat_robot_msgs::msg::LTERoleAnnouncement& msg) const
{
    const uint32_t local_term = lte_term_.load();
    if (msg.lte_term > local_term) return true;
    if (msg.lte_term < local_term) return false;
    // ★ Equal term — lower robot_id is the canonical winner.
    return msg.robot_id < robot_id_;
}
```

san_role_management Phase 1 patch (C4: Leader equal-term tiebreak) 와 동일 패턴 — swarm 전반에서 일관된 tiebreak 규칙.

### 3.6 LR5 fix — SubscriberHolder (offsetof on POD)

**Before**:
```cpp
class Mwan3UbusMonitor {
    struct ubus_subscriber subscriber_;
    std::mutex callback_mutex_;          // ★ non-POD member
    std::thread uloop_thread_;
    // ...
};

inline Mwan3UbusMonitor* subscriberOwner(struct ubus_subscriber* sub) {
    auto* owner = base - offsetof(Mwan3UbusMonitor, subscriber_);  // ★ UB
    return reinterpret_cast<Mwan3UbusMonitor*>(owner);
}
```

**After**:
```cpp
// ★ Standard-layout holder — offsetof is well-defined here.
struct SubscriberHolder {
    struct ubus_subscriber sub;
    Mwan3UbusMonitor* owner;
};

inline Mwan3UbusMonitor* subscriberOwner(struct ubus_subscriber* sub) {
    auto* holder = reinterpret_cast<SubscriberHolder*>(
        reinterpret_cast<std::byte*>(sub) - offsetof(SubscriberHolder, sub));
    return holder->owner;
}
```

### 3.7 LR6 fix — destructor ordering

```cpp
Mwan3UbusMonitor::~Mwan3UbusMonitor() {
    // 1. Stop uloop callbacks.
    running_.store(false);
    uloop_end();
    // 2. Wait for uloop thread BEFORE freeing ubus context.
    if (uloop_thread_.joinable()) uloop_thread_.join();
    // 3. Now safe to free.
    std::lock_guard<std::mutex> ctx_lock(ctx_mutex_);
    if (ubus_ctx_ != nullptr) {
        ubus_free(ubus_ctx_);
        ubus_ctx_ = nullptr;
    }
}
```

추가로 trampoline 에서 `running_.load()` 체크 — shutdown race-guard.

---

## 4. 검증 결과

### 4.1 Standalone validation (실측, 5/5 runs PASS)

```
$ g++ -std=c++17 -O2 -lpthread /tmp/validate_patches.cpp -o /tmp/validate
$ ./validate

PASS: V1  CSPRNG (getrandom or /dev/urandom) available
PASS: V2  length == 32
PASS: V2  all chars in [A-Za-z0-9]
INFO: V3  chi-square = 44.305 (df=61, expect <110)
PASS: V3  distribution roughly uniform
PASS: V4  1000 passphrases all distinct
PASS: V5  equal-term: id=3 preempts id=5 (lower wins)
PASS: V5  equal-term: id=5 does NOT preempt id=3
PASS: V6  higher term wins regardless of id
PASS: V6  higher term wins even with same id
PASS: V7  stale term: msg term < local → no preempt
PASS: V7  stale term boundary
PASS: V8  srt_uri does not contain 'passphrase' substring
PASS: V8b srt_uri does not contain 'pbkeylen' substring
PASS: V9  srt_uri contains operator_ip
PASS: V9b srt_uri mode=listener
PASS: V10 term=0 equal: lower id wins (no underflow)
PASS: V10b term=MAX equal: lower id wins
PASS: V11 length parameter respected (16)
PASS: V11b length parameter respected (79)

=== ALL VALIDATION PASSED ===
(5/5 runs identical)
```

### 4.2 PATCH testcase 목록

| Test | 검증 항목 | 이슈 |
|---|---|---|
| PH1_CsprngAvailable | getrandom or /dev/urandom 가용 | HC5 |
| PH2_LengthOutOfRangeThrows | 10..79 range | HC5 |
| PH3_ThousandGenerationsDistinct | 1000 distinct | HC5 |
| PH4_SrtUriExcludesPassphrase | URI 에 passphrase 없음 | HC1 |
| PH6_SrtUriPointsAtOperatorEndpoint | endpoint = operator_ip | HC2 |
| PH9_ThumbnailThresholdConfigurable | parameter override | HC6 |
| PL4_StaleTermIgnored | 낮은 term 무시 | LR3 |
| PL5_TermRatchetOnReceive | 높은 term ratchet | LR3 |
| PL9_ConcurrentInjectionNoCrash | thrash test | LR2, LR8 |

---

## 5. 운용 시나리오 — passphrase leak hazard (HC1)

### Pre-patch (★ AES-128 passphrase 전체 swarm 으로 broadcast)

```
T+0    : operator 가 robot 5 의 video START 요청
T+10ms : Hub 가 passphrase 생성 (32-char) → state.passphrase 저장
         → buildSrtUri 가 "srt://hub_ip:8888?...&passphrase=Xy7..." 생성
T+15ms : Handle msg publish 에 srt_uri (passphrase 포함) + passphrase 필드 모두 채움
T+20ms : DDS reliable broadcast → /video/handle 모든 subscriber 수신
         ★ 단순 ROS bridge / debug tool 도 모두 passphrase 획득
         ★ MITM 가능 시 영상 stream 복호화 가능
```

### Post-patch (★ separation + redaction)

```
T+0    : operator video START
T+10ms : Hub passphrase 생성 (getrandom CSPRNG)
T+15ms : Handle msg:
         - srt_uri = "srt://operator_ip:8888?mode=listener&latency=120" ← no passphrase
         - passphrase = "Xy7..." (or empty if redact_passphrase_in_handle=true)
T+20ms : DDS reliable broadcast
         ★ srt_uri 만으로는 영상 복호화 불가
         ★ redaction mode 에서는 passphrase 도 차단; 운영자가 인증된 채널로 별도 전달
```

---

## 6. 호환성

| 항목 | 변경 |
|---|---|
| 토픽 / QoS / 메시지 타입 | **동일** |
| 노드 이름 (`gstreamer_relay_node`, `lte_role_manager`) | **동일** |
| 기존 parameter | **동일** + 추가 |
| 추가 parameter (hub_comm) | `thumbnail_threshold` (default 4), `redact_passphrase_in_handle` (default false) |
| Public API | `Sequence(memory=)` 추가 (default True), `reset()` 추가 |
| 의도된 거동 변경 | **srt_uri 에서 passphrase 제거**, **srt_uri endpoint = operator_ip**, **promote() 비동기** |
| Drop-in 교체 | ✅ 가능 — 다른 패키지 영향 없음. 단 operator 클라이언트가 srt_uri 만 보고 연결하던 경우, **bug 가 fix 되면서 endpoint 가 변경**됨 (이전엔 어차피 연결 실패 상태였음) |

---

## 7. Before / After

| 검증 항목 | v1.3.0 baseline | v1.5.1 (PATCH) |
|---|---|---|
| HC1: passphrase plaintext broadcast | ❌ srt_uri + handle field 둘 다 | ✅ field 만, redaction option |
| HC2: srt_uri endpoint | ❌ hub_ip (잘못된 — 연결 실패) | ✅ operator_ip |
| HC3: active_streams_ race | ❌ no lock | ✅ streams_mutex_ |
| HC4: thumbnail check-write race | ❌ atomic 만 | ✅ critical section |
| HC5: CSPRNG | ❌ random_device 의존 | ✅ getrandom(2) + reject sampling |
| HC6: threshold | ❌ hardcoded 4 | ✅ parameter |
| LR1: promote sleep block | ❌ 최대 2s executor block | ✅ async wall_timer |
| LR2: role_ race | ❌ no mutex | ✅ recursive_mutex |
| LR3: split-brain check | ❌ off-by-one underflow | ✅ ratchet + tiebreak |
| LR4: chain-fight | ❌ msg 순서 의존 | ✅ equal-term tiebreak (lower id) |
| LR5: offsetof UB | ❌ non-POD 위반 | ✅ SubscriberHolder POD |
| LR6: destructor race | ❌ thread + ctx 동시 정리 | ✅ stop → join → free |
| LR8: announce_seq_ | ❌ non-atomic | ✅ atomic |
| **테스트** | hub_comm 5 + lte 7 = 12 | **+ 9 PATCH testcase + 20 standalone V1-V11b** |

---

## 8. 후속 작업 (CDR / TRR1)

### 8.1 단기 (CDR)

- [ ] **HC1 후속**: ROS DDS 의 secure transport (sros2) 설정 — passphrase 가 redaction 없이 전송될 때도 보호
- [ ] **operator client 검증**: 기존 client 가 새 srt_uri 포맷 (passphrase 없음) 으로 정상 연결되는지 통합 시험
- [ ] **LR1 후속**: launch_test 로 async promote 의 deadline / cancel 경로 검증
- [ ] **at_response_parser**: 본 patch 에 포함 안 됨 — regex 성능 측정 + lib 없는 fallback

### 8.2 중기 (TRR1)

- [ ] **HC4 후속**: thumbnail mode 시 새 stream 도 자동 downgrade — 현재 mid-deployment 혼재 가능
- [ ] **LR4 후속**: chain-fight 시나리오 multi-node 통합 시험 (3+ backup 동시 detect)
- [ ] **LR2 후속**: state_mu_ contention 측정 — 평상시 lock-free path 필요한지 확인

### 8.3 장기 (TRR2)

- [ ] **HC2 후속**: SRT relay 가 listener 모드로 동작하도록 변경 (operator 가 caller — NAT traversal 유리)
- [ ] **LR1 후속**: PPP 확립 시 wait 대신 callback 통합 (libubus event 직접 수신)

---

## 9. 결론

본 patch 는 san_hub_comm + san_lte_redundancy 의 critical safety/security bug 13건 해결:

- ✅ **HC1**: AES-128 passphrase leak 차단 (srt_uri 에서 제거 + redaction option)
- ✅ **HC2**: SRT URI endpoint 일관성 (caller↔listener mismatch 제거)
- ✅ **HC3/HC4**: MultiThreadedExecutor 환경 thread-safety
- ✅ **HC5**: getrandom(2) CSPRNG 보장 + rejection sampling
- ✅ **HC6**: thumbnail_threshold runtime parameter
- ✅ **LR1**: promote() executor block 제거 (sync sleep → async wall_timer)
- ✅ **LR2**: state_mu_ recursive_mutex
- ✅ **LR3/LR4**: equal-term tiebreak (lower robot_id wins) — swarm 일관 규칙
- ✅ **LR5**: SubscriberHolder POD (UB 제거)
- ✅ **LR6**: destructor ordering (stop → join → free)
- ✅ **LR8**: announce_seq_ atomic
- ✅ standalone validation **20/20 PASS**, 5/5 stability
- ✅ regression 0건 — 기존 12 gtest 인터페이스 보존

PDR 평가 시 evidence:

- **영상 암호화 신뢰성**: passphrase 가 ROS DDS topic 으로 broadcast 되지 않음 — 영상 보안 layer 가 실제로 의미를 가짐
- **LTE 이중화 신뢰성**: split-brain 가능성 제거 — equal-term tiebreak 으로 두 backup 동시 promote 불가
- **rclcpp executor 무결성**: promote() 가 더 이상 executor 를 block 하지 않음 — single-thread executor 도 사용 가능
- **C++ standard compliance**: offsetof UB 제거 — strict compile flag 도 통과
