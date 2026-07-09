# Third-party notices — `tracking_lib`

This directory vendors third-party multi-object-tracking code. The files are
included under their original MIT licenses; SAN's proprietary header does
**not** apply to them.

## ByteTrack (C++ port)

`BYTETracker.{h,cpp}`, `STrack.{h,cpp}`, `KalmanFilter.{h,cpp}`,
`Rect.{h,cpp}`, `Object.{h,cpp}`

Derived from the ByteTrack algorithm and its C++ port:

- ByteTrack — "ByteTrack: Multi-Object Tracking by Associating Every Detection
  Box", Yifu Zhang et al. (2022). https://github.com/ifzhang/ByteTrack
- C++ port: https://github.com/Vertical-Beach/ByteTrack-cpp

Licensed under the MIT License (see below).

## lapjv — linear assignment (Jonker-Volgenant)

`lapjv.{h,cpp}`

Jonker-Volgenant linear assignment solver by Tomas Kazmar.
https://github.com/gatagat/lapjv — MIT License.

## MIT License

```
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
