#include "ByteTracker.h"
#include "ReIDExtractor.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <chrono>

#include "../core/Logger.h"
using namespace AISecurityVision;
// Track implementation
ByteTracker::Track::Track(int id, const cv::Rect& box, float conf, int cls)
    : trackId(id), bbox(box), confidence(conf), classId(cls)
    , state(TrackState::New), framesSinceUpdate(0), age(0)
    , velocity(0, 0), hasReIDFeatures(false), lastReIDUpdate(0) {

    // Initialize Kalman filter for this track
    kalmanFilter.init(8, 4, 0); // 8 state variables, 4 measurements

    // State: [x, y, w, h, vx, vy, vw, vh]
    kalmanFilter.statePre = cv::Mat::zeros(8, 1, CV_32F);
    kalmanFilter.statePre.at<float>(0) = box.x + box.width / 2.0f;  // center x
    kalmanFilter.statePre.at<float>(1) = box.y + box.height / 2.0f; // center y
    kalmanFilter.statePre.at<float>(2) = box.width;
    kalmanFilter.statePre.at<float>(3) = box.height;

    // Transition matrix (constant velocity model)
    kalmanFilter.transitionMatrix = cv::Mat::eye(8, 8, CV_32F);
    kalmanFilter.transitionMatrix.at<float>(0, 4) = 1; // x += vx
    kalmanFilter.transitionMatrix.at<float>(1, 5) = 1; // y += vy
    kalmanFilter.transitionMatrix.at<float>(2, 6) = 1; // w += vw
    kalmanFilter.transitionMatrix.at<float>(3, 7) = 1; // h += vh

    // Measurement matrix
    kalmanFilter.measurementMatrix = cv::Mat::zeros(4, 8, CV_32F);
    kalmanFilter.measurementMatrix.at<float>(0, 0) = 1; // measure x
    kalmanFilter.measurementMatrix.at<float>(1, 1) = 1; // measure y
    kalmanFilter.measurementMatrix.at<float>(2, 2) = 1; // measure w
    kalmanFilter.measurementMatrix.at<float>(3, 3) = 1; // measure h

    // Process noise covariance
    cv::setIdentity(kalmanFilter.processNoiseCov, cv::Scalar::all(1e-2));

    // Measurement noise covariance
    cv::setIdentity(kalmanFilter.measurementNoiseCov, cv::Scalar::all(1e-1));

    // Error covariance
    cv::setIdentity(kalmanFilter.errorCovPost, cv::Scalar::all(1));
}

void ByteTracker::Track::predict() {
    cv::Mat prediction = kalmanFilter.predict();

    // Update predicted bbox
    float cx = prediction.at<float>(0);
    float cy = prediction.at<float>(1);
    float w = prediction.at<float>(2);
    float h = prediction.at<float>(3);

    bbox = cv::Rect(cx - w/2, cy - h/2, w, h);

    // Update velocity
    velocity.x = prediction.at<float>(4);
    velocity.y = prediction.at<float>(5);

    framesSinceUpdate++;
    age++;
}

void ByteTracker::Track::update(const cv::Rect& box, float conf) {
    // Measurement vector
    cv::Mat measurement = cv::Mat::zeros(4, 1, CV_32F);
    measurement.at<float>(0) = box.x + box.width / 2.0f;  // center x
    measurement.at<float>(1) = box.y + box.height / 2.0f; // center y
    measurement.at<float>(2) = box.width;
    measurement.at<float>(3) = box.height;

    // Update Kalman filter
    kalmanFilter.correct(measurement);

    // Update track properties
    bbox = box;
    confidence = conf;
    framesSinceUpdate = 0;
    age++;

    // Update state
    if (state == TrackState::New) {
        state = TrackState::Tracked;
    } else if (state == TrackState::Lost) {
        state = TrackState::Tracked;
    }
}

cv::Rect ByteTracker::Track::getPredictedBbox() const {
    return bbox;
}

void ByteTracker::Track::updateReIDFeatures(const std::vector<float>& features) {
    if (!features.empty()) {
        reidFeatures = features;
        hasReIDFeatures = true;
        lastReIDUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
}

bool ByteTracker::Track::hasValidReIDFeatures() const {
    return hasReIDFeatures && !reidFeatures.empty();
}

float ByteTracker::Track::computeReIDSimilarity(const Track& other) const {
    if (!hasValidReIDFeatures() || !other.hasValidReIDFeatures()) {
        return 0.0f;
    }

    // Compute cosine similarity
    float dotProduct = 0.0f;
    float norm1 = 0.0f;
    float norm2 = 0.0f;

    for (size_t i = 0; i < reidFeatures.size() && i < other.reidFeatures.size(); ++i) {
        dotProduct += reidFeatures[i] * other.reidFeatures[i];
        norm1 += reidFeatures[i] * reidFeatures[i];
        norm2 += other.reidFeatures[i] * other.reidFeatures[i];
    }

    if (norm1 == 0.0f || norm2 == 0.0f) {
        return 0.0f;
    }

    return dotProduct / (std::sqrt(norm1) * std::sqrt(norm2));
}

// ByteTracker implementation
ByteTracker::ByteTracker()
    : m_trackThreshold(0.5f)
    , m_highThreshold(0.6f)
    , m_matchThreshold(0.8f)
    , m_maxLostFrames(30)
    , m_minTrackLength(3)
    , m_reidSimilarityThreshold(0.7f)
    , m_reidWeight(0.3f)
    , m_reidTrackingEnabled(false)
    , m_nextTrackId(1)
    , m_frameCount(0)
    , m_totalTracks(0) {
}

ByteTracker::~ByteTracker() {
    cleanup();
}

bool ByteTracker::initialize() {
    LOG_INFO() << "[ByteTracker] Initializing ByteTracker...";
    LOG_INFO() << "[ByteTracker] Track threshold: " << m_trackThreshold;
    LOG_INFO() << "[ByteTracker] High threshold: " << m_highThreshold;
    LOG_INFO() << "[ByteTracker] Match threshold: " << m_matchThreshold;
    LOG_INFO() << "[ByteTracker] Max lost frames: " << m_maxLostFrames;
    LOG_INFO() << "[ByteTracker] ByteTracker initialized successfully";
    return true;
}

void ByteTracker::cleanup() {
    clearTracks();
    LOG_INFO() << "[ByteTracker] Cleanup completed";
}

std::vector<int> ByteTracker::update(const std::vector<cv::Rect>& detections) {
    std::vector<float> confidences(detections.size(), 1.0f);
    return updateWithConfidence(detections, confidences);
}

std::vector<int> ByteTracker::updateWithConfidence(const std::vector<cv::Rect>& detections,
                                                  const std::vector<float>& confidences) {
    std::vector<int> classIds(detections.size(), 0); // Default to class 0
    return updateWithClasses(detections, confidences, classIds);
}

std::vector<int> ByteTracker::updateWithClasses(const std::vector<cv::Rect>& detections,
                                               const std::vector<float>& confidences,
                                               const std::vector<int>& classIds) {
    m_frameCount++;

    // Predict all existing tracks
    predictTracks();

    // Associate detections with tracks
    associateDetections(detections, confidences, classIds);

    // Update track states and remove dead tracks
    updateTrackStates();
    removeDeadTracks();

    // Return track IDs for active tracks
    std::vector<int> trackIds;
    for (const auto& track : m_activeTracks) {
        trackIds.push_back(track->trackId);
    }

    return trackIds;
}

// Track management methods
std::vector<std::shared_ptr<ByteTracker::Track>> ByteTracker::getActiveTracks() const {
    return m_activeTracks;
}

std::shared_ptr<ByteTracker::Track> ByteTracker::getTrack(int trackId) const {
    auto it = m_tracks.find(trackId);
    return (it != m_tracks.end()) ? it->second : nullptr;
}

void ByteTracker::removeTrack(int trackId) {
    auto it = m_tracks.find(trackId);
    if (it != m_tracks.end()) {
        // Remove from active tracks
        m_activeTracks.erase(
            std::remove(m_activeTracks.begin(), m_activeTracks.end(), it->second),
            m_activeTracks.end());

        // Remove from lost tracks
        m_lostTracks.erase(
            std::remove(m_lostTracks.begin(), m_lostTracks.end(), it->second),
            m_lostTracks.end());

        // Remove from main tracks map
        m_tracks.erase(it);
    }
}

void ByteTracker::clearTracks() {
    m_tracks.clear();
    m_activeTracks.clear();
    m_lostTracks.clear();
    m_nextTrackId = 1;
    m_frameCount = 0;
}

// Configuration methods
void ByteTracker::setTrackThreshold(float threshold) {
    m_trackThreshold = std::max(0.0f, std::min(1.0f, threshold));
}

void ByteTracker::setHighThreshold(float threshold) {
    m_highThreshold = std::max(0.0f, std::min(1.0f, threshold));
}

void ByteTracker::setMatchThreshold(float threshold) {
    m_matchThreshold = std::max(0.0f, std::min(1.0f, threshold));
}

void ByteTracker::setMaxLostFrames(int frames) {
    m_maxLostFrames = std::max(1, frames);
}

void ByteTracker::setMinTrackLength(int length) {
    m_minTrackLength = std::max(1, length);
}

// ReID configuration methods
void ByteTracker::setReIDSimilarityThreshold(float threshold) {
    m_reidSimilarityThreshold = std::max(0.0f, std::min(1.0f, threshold));
    LOG_INFO() << "[ByteTracker] ReID similarity threshold set to: " << m_reidSimilarityThreshold;
}

void ByteTracker::setReIDWeight(float weight) {
    m_reidWeight = std::max(0.0f, std::min(1.0f, weight));
    LOG_INFO() << "[ByteTracker] ReID weight set to: " << m_reidWeight;
}

void ByteTracker::enableReIDTracking(bool enabled) {
    m_reidTrackingEnabled = enabled;
    LOG_INFO() << "[ByteTracker] ReID tracking " << (enabled ? "enabled" : "disabled");
}

// Statistics methods
size_t ByteTracker::getActiveTrackCount() const {
    return m_activeTracks.size();
}

size_t ByteTracker::getTotalTrackCount() const {
    return m_totalTracks;
}

float ByteTracker::getAverageTrackLength() const {
    if (m_trackLengths.empty()) {
        return 0.0f;
    }

    int sum = std::accumulate(m_trackLengths.begin(), m_trackLengths.end(), 0);
    return static_cast<float>(sum) / m_trackLengths.size();
}

// Internal methods
std::vector<std::vector<float>> ByteTracker::computeIoUMatrix(
    const std::vector<cv::Rect>& detections,
    const std::vector<std::shared_ptr<Track>>& tracks) {

    std::vector<std::vector<float>> iouMatrix(detections.size(),
                                             std::vector<float>(tracks.size(), 0.0f));

    for (size_t i = 0; i < detections.size(); ++i) {
        for (size_t j = 0; j < tracks.size(); ++j) {
            iouMatrix[i][j] = computeIoU(detections[i], tracks[j]->getPredictedBbox());
        }
    }

    return iouMatrix;
}

std::vector<std::pair<int, int>> ByteTracker::hungarianAssignment(
    const std::vector<std::vector<float>>& costMatrix) {

    // Simplified Hungarian algorithm implementation
    // In a full implementation, this would use the Hungarian algorithm
    // For now, use a greedy approach

    std::vector<std::pair<int, int>> assignments;
    std::vector<bool> detectionUsed(costMatrix.size(), false);
    std::vector<bool> trackUsed(costMatrix.empty() ? 0 : costMatrix[0].size(), false);

    // Find best matches greedily
    for (int iter = 0; iter < static_cast<int>(costMatrix.size()); ++iter) {
        float bestScore = 0.0f;
        int bestDet = -1, bestTrack = -1;

        for (size_t i = 0; i < costMatrix.size(); ++i) {
            if (detectionUsed[i]) continue;

            for (size_t j = 0; j < costMatrix[i].size(); ++j) {
                if (trackUsed[j]) continue;

                if (costMatrix[i][j] > bestScore && costMatrix[i][j] > m_matchThreshold) {
                    bestScore = costMatrix[i][j];
                    bestDet = i;
                    bestTrack = j;
                }
            }
        }

        if (bestDet >= 0 && bestTrack >= 0) {
            assignments.push_back({bestDet, bestTrack});
            detectionUsed[bestDet] = true;
            trackUsed[bestTrack] = true;
        } else {
            break;
        }
    }

    return assignments;
}

void ByteTracker::predictTracks() {
    for (auto& track : m_activeTracks) {
        track->predict();
    }

    for (auto& track : m_lostTracks) {
        track->predict();
    }
}

void ByteTracker::associateDetections(const std::vector<cv::Rect>& detections,
                                     const std::vector<float>& confidences,
                                     const std::vector<int>& classIds) {

    // Separate high and low confidence detections
    std::vector<cv::Rect> highDetections, lowDetections;
    std::vector<float> highConfidences, lowConfidences;
    std::vector<int> highClassIds, lowClassIds;
    std::vector<int> highIndices, lowIndices;

    for (size_t i = 0; i < detections.size(); ++i) {
        if (confidences[i] >= m_highThreshold) {
            highDetections.push_back(detections[i]);
            highConfidences.push_back(confidences[i]);
            highClassIds.push_back(classIds[i]);
            highIndices.push_back(i);
        } else if (confidences[i] >= m_trackThreshold) {
            lowDetections.push_back(detections[i]);
            lowConfidences.push_back(confidences[i]);
            lowClassIds.push_back(classIds[i]);
            lowIndices.push_back(i);
        }
    }

    // First association: high confidence detections with active tracks
    auto iouMatrix = computeIoUMatrix(highDetections, m_activeTracks);
    auto assignments = hungarianAssignment(iouMatrix);

    std::vector<bool> detectionMatched(highDetections.size(), false);
    std::vector<bool> trackMatched(m_activeTracks.size(), false);

    // Update matched tracks
    for (const auto& assignment : assignments) {
        int detIdx = assignment.first;
        int trackIdx = assignment.second;

        m_activeTracks[trackIdx]->update(highDetections[detIdx], highConfidences[detIdx]);
        detectionMatched[detIdx] = true;
        trackMatched[trackIdx] = true;
    }

    // Move unmatched active tracks to lost
    for (size_t i = 0; i < m_activeTracks.size(); ++i) {
        if (!trackMatched[i]) {
            m_activeTracks[i]->state = TrackState::Lost;
            m_lostTracks.push_back(m_activeTracks[i]);
        }
    }

    // Remove matched tracks from active list
    auto newEnd = std::remove_if(m_activeTracks.begin(), m_activeTracks.end(),
        [](const std::shared_ptr<Track>& track) {
            return track->state == TrackState::Lost;
        });
    m_activeTracks.erase(newEnd, m_activeTracks.end());

    // Create new tracks from unmatched high confidence detections
    std::vector<cv::Rect> unmatchedDetections;
    std::vector<float> unmatchedConfidences;
    std::vector<int> unmatchedClassIds;

    for (size_t i = 0; i < highDetections.size(); ++i) {
        if (!detectionMatched[i]) {
            unmatchedDetections.push_back(highDetections[i]);
            unmatchedConfidences.push_back(highConfidences[i]);
            unmatchedClassIds.push_back(highClassIds[i]);
        }
    }

    initNewTracks(unmatchedDetections, unmatchedConfidences, unmatchedClassIds);
}

void ByteTracker::initNewTracks(const std::vector<cv::Rect>& unmatched_detections,
                               const std::vector<float>& confidences,
                               const std::vector<int>& classIds) {

    for (size_t i = 0; i < unmatched_detections.size(); ++i) {
        auto newTrack = std::make_shared<Track>(m_nextTrackId++,
                                               unmatched_detections[i],
                                               confidences[i],
                                               classIds[i]);

        m_tracks[newTrack->trackId] = newTrack;
        m_activeTracks.push_back(newTrack);
        m_totalTracks++;
    }
}

void ByteTracker::updateTrackStates() {
    // Update lost tracks
    for (auto& track : m_lostTracks) {
        if (track->framesSinceUpdate > m_maxLostFrames) {
            track->state = TrackState::Removed;
        }
    }
}

void ByteTracker::removeDeadTracks() {
    // Remove tracks that have been lost too long
    auto newEnd = std::remove_if(m_lostTracks.begin(), m_lostTracks.end(),
        [this](const std::shared_ptr<Track>& track) {
            if (track->state == TrackState::Removed) {
                // Record track length for statistics
                if (track->age >= m_minTrackLength) {
                    m_trackLengths.push_back(track->age);
                }

                // Remove from main tracks map
                m_tracks.erase(track->trackId);
                return true;
            }
            return false;
        });
    m_lostTracks.erase(newEnd, m_lostTracks.end());
}

float ByteTracker::computeIoU(const cv::Rect& box1, const cv::Rect& box2) const {
    cv::Rect intersection = box1 & box2;
    float intersectionArea = intersection.area();

    if (intersectionArea == 0) {
        return 0.0f;
    }

    float unionArea = box1.area() + box2.area() - intersectionArea;
    return intersectionArea / unionArea;
}

cv::KalmanFilter ByteTracker::createKalmanFilter(const cv::Rect& bbox) const {
    cv::KalmanFilter kf(8, 4, 0);

    // Initialize state with bbox center and size
    kf.statePre.at<float>(0) = bbox.x + bbox.width / 2.0f;
    kf.statePre.at<float>(1) = bbox.y + bbox.height / 2.0f;
    kf.statePre.at<float>(2) = bbox.width;
    kf.statePre.at<float>(3) = bbox.height;

    // Set up matrices (same as in Track constructor)
    kf.transitionMatrix = cv::Mat::eye(8, 8, CV_32F);
    kf.transitionMatrix.at<float>(0, 4) = 1;
    kf.transitionMatrix.at<float>(1, 5) = 1;
    kf.transitionMatrix.at<float>(2, 6) = 1;
    kf.transitionMatrix.at<float>(3, 7) = 1;

    kf.measurementMatrix = cv::Mat::zeros(4, 8, CV_32F);
    kf.measurementMatrix.at<float>(0, 0) = 1;
    kf.measurementMatrix.at<float>(1, 1) = 1;
    kf.measurementMatrix.at<float>(2, 2) = 1;
    kf.measurementMatrix.at<float>(3, 3) = 1;

    cv::setIdentity(kf.processNoiseCov, cv::Scalar::all(1e-2));
    cv::setIdentity(kf.measurementNoiseCov, cv::Scalar::all(1e-1));
    cv::setIdentity(kf.errorCovPost, cv::Scalar::all(1));

    return kf;
}

// ReID-enhanced tracking methods
std::vector<int> ByteTracker::updateWithReIDFeatures(const std::vector<cv::Rect>& detections,
                                                    const std::vector<float>& confidences,
                                                    const std::vector<int>& classIds,
                                                    const std::vector<std::vector<float>>& reidFeatures) {
    m_frameCount++;

    // Predict all existing tracks
    predictTracks();

    // Associate detections with tracks using ReID features
    if (m_reidTrackingEnabled && !reidFeatures.empty()) {
        associateDetectionsWithReID(detections, confidences, classIds, reidFeatures);
    } else {
        associateDetections(detections, confidences, classIds);
    }

    // Update track states and remove dead tracks
    updateTrackStates();
    removeDeadTracks();

    // Return track IDs for active tracks
    std::vector<int> trackIds;
    for (const auto& track : m_activeTracks) {
        trackIds.push_back(track->trackId);
    }

    return trackIds;
}

void ByteTracker::associateDetectionsWithReID(const std::vector<cv::Rect>& detections,
                                             const std::vector<float>& confidences,
                                             const std::vector<int>& classIds,
                                             const std::vector<std::vector<float>>& reidFeatures) {

    // Separate high and low confidence detections
    std::vector<cv::Rect> highDetections, lowDetections;
    std::vector<float> highConfidences, lowConfidences;
    std::vector<int> highClassIds, lowClassIds;
    std::vector<std::vector<float>> highReIDFeatures, lowReIDFeatures;

    for (size_t i = 0; i < detections.size(); ++i) {
        if (confidences[i] >= m_highThreshold) {
            highDetections.push_back(detections[i]);
            highConfidences.push_back(confidences[i]);
            highClassIds.push_back(classIds[i]);
            if (i < reidFeatures.size()) {
                highReIDFeatures.push_back(reidFeatures[i]);
            }
        } else if (confidences[i] >= m_trackThreshold) {
            lowDetections.push_back(detections[i]);
            lowConfidences.push_back(confidences[i]);
            lowClassIds.push_back(classIds[i]);
            if (i < reidFeatures.size()) {
                lowReIDFeatures.push_back(reidFeatures[i]);
            }
        }
    }

    // Compute IoU and ReID similarity matrices
    auto iouMatrix = computeIoUMatrix(highDetections, m_activeTracks);
    auto reidMatrix = computeReIDSimilarityMatrix(highReIDFeatures, m_activeTracks);

    // Combine IoU and ReID similarities
    auto combinedMatrix = computeCombinedCostMatrix(iouMatrix, reidMatrix);
    auto assignments = hungarianAssignment(combinedMatrix);

    std::vector<bool> detectionMatched(highDetections.size(), false);
    std::vector<bool> trackMatched(m_activeTracks.size(), false);

    // Update matched tracks with ReID features
    for (const auto& assignment : assignments) {
        int detIdx = assignment.first;
        int trackIdx = assignment.second;

        m_activeTracks[trackIdx]->update(highDetections[detIdx], highConfidences[detIdx]);

        // Update ReID features if available
        if (detIdx < static_cast<int>(highReIDFeatures.size())) {
            m_activeTracks[trackIdx]->updateReIDFeatures(highReIDFeatures[detIdx]);
        }

        detectionMatched[detIdx] = true;
        trackMatched[trackIdx] = true;
    }

    // Move unmatched active tracks to lost
    for (size_t i = 0; i < m_activeTracks.size(); ++i) {
        if (!trackMatched[i]) {
            m_activeTracks[i]->state = TrackState::Lost;
            m_lostTracks.push_back(m_activeTracks[i]);
        }
    }

    // Remove matched tracks from active list
    auto newEnd = std::remove_if(m_activeTracks.begin(), m_activeTracks.end(),
        [](const std::shared_ptr<Track>& track) {
            return track->state == TrackState::Lost;
        });
    m_activeTracks.erase(newEnd, m_activeTracks.end());

    // Create new tracks from unmatched high confidence detections
    std::vector<cv::Rect> unmatchedDetections;
    std::vector<float> unmatchedConfidences;
    std::vector<int> unmatchedClassIds;
    std::vector<std::vector<float>> unmatchedReIDFeatures;

    for (size_t i = 0; i < highDetections.size(); ++i) {
        if (!detectionMatched[i]) {
            unmatchedDetections.push_back(highDetections[i]);
            unmatchedConfidences.push_back(highConfidences[i]);
            unmatchedClassIds.push_back(highClassIds[i]);
            if (i < highReIDFeatures.size()) {
                unmatchedReIDFeatures.push_back(highReIDFeatures[i]);
            }
        }
    }

    initNewTracksWithReID(unmatchedDetections, unmatchedConfidences, unmatchedClassIds, unmatchedReIDFeatures);
}

void ByteTracker::initNewTracksWithReID(const std::vector<cv::Rect>& unmatched_detections,
                                       const std::vector<float>& confidences,
                                       const std::vector<int>& classIds,
                                       const std::vector<std::vector<float>>& reidFeatures) {

    for (size_t i = 0; i < unmatched_detections.size(); ++i) {
        auto newTrack = std::make_shared<Track>(m_nextTrackId++,
                                               unmatched_detections[i],
                                               confidences[i],
                                               classIds[i]);

        // Add ReID features if available
        if (i < reidFeatures.size() && !reidFeatures[i].empty()) {
            newTrack->updateReIDFeatures(reidFeatures[i]);
        }

        m_tracks[newTrack->trackId] = newTrack;
        m_activeTracks.push_back(newTrack);
        m_totalTracks++;
    }
}

// ReID utility methods
std::vector<std::vector<float>> ByteTracker::computeReIDSimilarityMatrix(
    const std::vector<std::vector<float>>& detectionFeatures,
    const std::vector<std::shared_ptr<Track>>& tracks) {

    std::vector<std::vector<float>> similarityMatrix(detectionFeatures.size(),
                                                    std::vector<float>(tracks.size(), 0.0f));

    for (size_t i = 0; i < detectionFeatures.size(); ++i) {
        for (size_t j = 0; j < tracks.size(); ++j) {
            if (!detectionFeatures[i].empty() && tracks[j]->hasValidReIDFeatures()) {
                similarityMatrix[i][j] = computeReIDSimilarity(detectionFeatures[i], tracks[j]->reidFeatures);
            }
        }
    }

    return similarityMatrix;
}

std::vector<std::vector<float>> ByteTracker::computeCombinedCostMatrix(
    const std::vector<std::vector<float>>& iouMatrix,
    const std::vector<std::vector<float>>& reidMatrix) {

    if (iouMatrix.size() != reidMatrix.size() ||
        (iouMatrix.size() > 0 && iouMatrix[0].size() != reidMatrix[0].size())) {
        return iouMatrix; // Fallback to IoU only
    }

    std::vector<std::vector<float>> combinedMatrix(iouMatrix.size(),
                                                  std::vector<float>(iouMatrix.empty() ? 0 : iouMatrix[0].size(), 0.0f));

    for (size_t i = 0; i < iouMatrix.size(); ++i) {
        for (size_t j = 0; j < iouMatrix[i].size(); ++j) {
            // Combine IoU and ReID similarity with weights
            float iouWeight = 1.0f - m_reidWeight;
            combinedMatrix[i][j] = iouWeight * iouMatrix[i][j] + m_reidWeight * reidMatrix[i][j];
        }
    }

    return combinedMatrix;
}

float ByteTracker::computeReIDSimilarity(const std::vector<float>& features1,
                                        const std::vector<float>& features2) const {
    if (features1.size() != features2.size() || features1.empty()) {
        return 0.0f;
    }

    float dotProduct = 0.0f;
    float norm1 = 0.0f;
    float norm2 = 0.0f;

    for (size_t i = 0; i < features1.size(); ++i) {
        dotProduct += features1[i] * features2[i];
        norm1 += features1[i] * features1[i];
        norm2 += features2[i] * features2[i];
    }

    if (norm1 == 0.0f || norm2 == 0.0f) {
        return 0.0f;
    }

    return dotProduct / (std::sqrt(norm1) * std::sqrt(norm2));
}
