#include "ByteTrack/STrack.h"
#include <cstddef>

namespace byte_track
{

STrack::STrack(const Rect<float>& rect, const float& score) :
    kalman_filter_(),
    mean_(),
    covariance_(),
    rect_(rect),
    state_(STrackState::New),
    is_activated_(false),
    score_(score),
    track_id_(0),
    frame_id_(0),
    start_frame_id_(0),
    tracklet_len_(0)
{
}

STrack::~STrack() {}

const Rect<float>& STrack::getRect() const
{
    return rect_;
}

// Implement the non-const version of getRect
Rect<float>& STrack::getRect()
{
    return rect_;
}

const STrackState& STrack::getSTrackState() const
{
    return state_;
}

const bool& STrack::isActivated() const
{
    return is_activated_;
}

const float& STrack::getScore() const
{
    return score_;
}

const size_t& STrack::getTrackId() const
{
    return track_id_;
}

const size_t& STrack::getFrameId() const
{
    return frame_id_;
}

const size_t& STrack::getStartFrameId() const
{
    return start_frame_id_;
}

const size_t& STrack::getTrackletLength() const
{
    return tracklet_len_;
}

void STrack::activate(const size_t& frame_id, const size_t& track_id)
{
    kalman_filter_.initiate(mean_, covariance_, rect_.getXyah());
    updateRect();
    state_ = STrackState::Tracked;
    if (frame_id == 1)
    {
        is_activated_ = true;
    }
    track_id_ = track_id;
    frame_id_ = frame_id;
    start_frame_id_ = frame_id;
    tracklet_len_ = 0;
}

void STrack::reActivate(const STrack &new_track, const size_t &frame_id, const int &new_track_id)
{
    //kalman_filter_.update(mean_, covariance_, new_track.getRect().getXyah());
    kalman_filter_.update(mean_, covariance_, new_track.getRect().getXyah(), new_track.getScore());
    updateRect();
    state_ = STrackState::Tracked;
    is_activated_ = true;
    score_ = new_track.getScore();
    if (0 <= new_track_id)
    {
        track_id_ = new_track_id;
    }
    frame_id_ = frame_id;
    tracklet_len_ = 0;
}

void STrack::predict()
{
    if (state_ != STrackState::Tracked)
    {
        mean_[7] = 0;
    }
    kalman_filter_.predict(mean_, covariance_);
}

void STrack::update(const STrack &new_track, const size_t &frame_id)
{
    //kalman_filter_.update(mean_, covariance_, new_track.getRect().getXyah());
    kalman_filter_.update(mean_, covariance_, new_track.getRect().getXyah(), new_track.getScore());
    updateRect();
    state_ = STrackState::Tracked;
    is_activated_ = true;
    score_ = new_track.getScore();
    frame_id_ = frame_id;
    tracklet_len_++;
}

void STrack::markAsLost()
{
    state_ = STrackState::Lost;
}

void STrack::markAsRemoved()
{
    state_ = STrackState::Removed;
}

void STrack::updateRect()
{
    rect_.width() = mean_[2] * mean_[3];
    rect_.height() = mean_[3];
    rect_.x() = mean_[0] - rect_.width() / 2;
    rect_.y() = mean_[1] - rect_.height() / 2;
}

} // end namespace byte_track
