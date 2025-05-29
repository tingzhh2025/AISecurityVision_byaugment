#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <memory>
#include <unordered_map>

// Forward declaration for ReID features
class ReIDExtractor;
struct ReIDEmbedding;

/**
 * @brief ByteTracker implementation for multi-object tracking
 *
 * This class implements the ByteTracker algorithm for robust multi-object tracking.
 * It uses Kalman filters for motion prediction and Hungarian algorithm for data association.
 *
 * Features:
 * - High-performance tracking with Kalman filters
 * - Robust data association using Hungarian algorithm
 * - Track lifecycle management (birth, active, lost, deleted)
 * - Support for different object classes
 * - Configurable tracking parameters
 */
class ByteTracker {
public:
    // Track state enumeration
    enum class TrackState {
        New = 0,
        Tracked = 1,
        Lost = 2,
        Removed = 3
    };

    // Track structure
    struct Track {
        int trackId;
        cv::Rect bbox;
        cv::Point2f velocity;
        float confidence;
        int classId;
        TrackState state;
        int framesSinceUpdate;
        int age;
        cv::KalmanFilter kalmanFilter;

        // ReID features for cross-camera tracking
        std::vector<float> reidFeatures;
        bool hasReIDFeatures;
        int64_t lastReIDUpdate;

        Track(int id, const cv::Rect& box, float conf, int cls);
        void predict();
        void update(const cv::Rect& box, float conf);
        void updateReIDFeatures(const std::vector<float>& features);
        cv::Rect getPredictedBbox() const;
        bool hasValidReIDFeatures() const;
        float computeReIDSimilarity(const Track& other) const;
    };

    ByteTracker();
    ~ByteTracker();

    // Initialization
    bool initialize();
    void cleanup();

    // Tracking
    std::vector<int> update(const std::vector<cv::Rect>& detections);
    std::vector<int> updateWithConfidence(const std::vector<cv::Rect>& detections,
                                         const std::vector<float>& confidences);
    std::vector<int> updateWithClasses(const std::vector<cv::Rect>& detections,
                                      const std::vector<float>& confidences,
                                      const std::vector<int>& classIds);

    // ReID-enhanced tracking
    std::vector<int> updateWithReIDFeatures(const std::vector<cv::Rect>& detections,
                                           const std::vector<float>& confidences,
                                           const std::vector<int>& classIds,
                                           const std::vector<std::vector<float>>& reidFeatures);

    // Track management
    std::vector<std::shared_ptr<Track>> getActiveTracks() const;
    std::shared_ptr<Track> getTrack(int trackId) const;
    void removeTrack(int trackId);
    void clearTracks();

    // Configuration
    void setTrackThreshold(float threshold);
    void setHighThreshold(float threshold);
    void setMatchThreshold(float threshold);
    void setMaxLostFrames(int frames);
    void setMinTrackLength(int length);

    // ReID configuration
    void setReIDSimilarityThreshold(float threshold);
    void setReIDWeight(float weight);
    void enableReIDTracking(bool enabled);

    // Statistics
    size_t getActiveTrackCount() const;
    size_t getTotalTrackCount() const;
    float getAverageTrackLength() const;

private:
    // Tracking parameters
    float m_trackThreshold;      // Threshold for track confirmation
    float m_highThreshold;       // High confidence threshold
    float m_matchThreshold;      // IoU threshold for matching
    int m_maxLostFrames;         // Max frames before track deletion
    int m_minTrackLength;        // Min length for valid track

    // ReID parameters
    float m_reidSimilarityThreshold;  // ReID similarity threshold
    float m_reidWeight;               // Weight for ReID in association
    bool m_reidTrackingEnabled;       // Enable/disable ReID tracking

    // Track management
    std::unordered_map<int, std::shared_ptr<Track>> m_tracks;
    std::vector<std::shared_ptr<Track>> m_activeTracks;
    std::vector<std::shared_ptr<Track>> m_lostTracks;
    int m_nextTrackId;
    int m_frameCount;

    // Statistics
    size_t m_totalTracks;
    std::vector<int> m_trackLengths;

    // Internal methods
    std::vector<std::vector<float>> computeIoUMatrix(
        const std::vector<cv::Rect>& detections,
        const std::vector<std::shared_ptr<Track>>& tracks);

    std::vector<std::pair<int, int>> hungarianAssignment(
        const std::vector<std::vector<float>>& costMatrix);

    void predictTracks();
    void associateDetections(const std::vector<cv::Rect>& detections,
                           const std::vector<float>& confidences,
                           const std::vector<int>& classIds);

    void associateDetectionsWithReID(const std::vector<cv::Rect>& detections,
                                    const std::vector<float>& confidences,
                                    const std::vector<int>& classIds,
                                    const std::vector<std::vector<float>>& reidFeatures);

    void initNewTracks(const std::vector<cv::Rect>& unmatched_detections,
                      const std::vector<float>& confidences,
                      const std::vector<int>& classIds);

    void initNewTracksWithReID(const std::vector<cv::Rect>& unmatched_detections,
                              const std::vector<float>& confidences,
                              const std::vector<int>& classIds,
                              const std::vector<std::vector<float>>& reidFeatures);

    void updateTrackStates();
    void removeDeadTracks();

    float computeIoU(const cv::Rect& box1, const cv::Rect& box2) const;
    cv::KalmanFilter createKalmanFilter(const cv::Rect& bbox) const;

    // ReID utility methods
    std::vector<std::vector<float>> computeReIDSimilarityMatrix(
        const std::vector<std::vector<float>>& detectionFeatures,
        const std::vector<std::shared_ptr<Track>>& tracks);

    std::vector<std::vector<float>> computeCombinedCostMatrix(
        const std::vector<std::vector<float>>& iouMatrix,
        const std::vector<std::vector<float>>& reidMatrix);

    float computeReIDSimilarity(const std::vector<float>& features1,
                               const std::vector<float>& features2) const;
};
