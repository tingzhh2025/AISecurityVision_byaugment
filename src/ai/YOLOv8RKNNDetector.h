/**
 * @file YOLOv8RKNNDetector.h
 * @brief YOLOv8 RKNN NPU Implementation
 *
 * This file implements YOLOv8 object detection using RKNN NPU acceleration.
 * Based on the reference implementation from /userdata/source/source/yolov8_rknn
 */

#ifndef YOLOV8_RKNN_DETECTOR_H
#define YOLOV8_RKNN_DETECTOR_H

#include "YOLOv8Detector.h"

#ifdef HAVE_RKNN
#include "rknn_api.h"
#endif

namespace AISecurityVision {

/**
 * @brief YOLOv8 detector implementation using RKNN NPU
 *
 * This class implements YOLOv8 object detection using Rockchip's RKNN NPU
 * for hardware acceleration on RK3588 and other compatible platforms.
 * Based on the official reference implementation structure.
 */
class YOLOv8RKNNDetector : public YOLOv8Detector {
public:
    YOLOv8RKNNDetector();
    virtual ~YOLOv8RKNNDetector();

    // YOLOv8Detector interface implementation
    bool initialize(const std::string& modelPath) override;
    std::vector<Detection> detectObjects(const cv::Mat& frame) override;
    bool isInitialized() const override;
    InferenceBackend getCurrentBackend() const override;
    std::string getBackendName() const override;
    void cleanup() override;
    std::vector<std::string> getModelInfo() const override;

    // RKNN-specific methods
    bool enableMultiCore(bool enable);
    void setZeroCopyMode(bool enable);

private:
#ifdef HAVE_RKNN
    // RKNN context and attributes
    rknn_context m_rknnContext;
    rknn_input_output_num m_ioNum;
    rknn_tensor_attr* m_inputAttrs;
    rknn_tensor_attr* m_outputAttrs;

    // Model properties
    bool m_isQuantized;
    bool m_multiCoreEnabled;
    bool m_zeroCopyMode;

    // Helper methods based on reference implementation
    cv::Mat preprocessImageWithLetterbox(const cv::Mat& image, LetterboxInfo& letterbox);
    std::vector<Detection> postprocessResults(rknn_output* outputs,
                                            rknn_tensor_attr* output_attrs,
                                            uint32_t n_output,
                                            const cv::Size& originalSize,
                                            const LetterboxInfo& letterbox);

    // Core processing functions (matching reference implementation)
    int processI8(int8_t* box_tensor, int32_t box_zp, float box_scale,
                  int8_t* score_tensor, int32_t score_zp, float score_scale,
                  int8_t* score_sum_tensor, int32_t score_sum_zp, float score_sum_scale,
                  int grid_h, int grid_w, int stride, int dfl_len,
                  std::vector<float>& boxes, std::vector<float>& objProbs,
                  std::vector<int>& classId, float threshold);

    // Utility functions (matching reference implementation)
    static float calculateOverlap(float xmin0, float ymin0, float xmax0, float ymax0,
                                 float xmin1, float ymin1, float xmax1, float ymax1);
    static int nms(int validCount, std::vector<float>& outputLocations,
                   std::vector<int>& classIds, std::vector<int>& order,
                   int filterId, float threshold);
    static int quickSortIndiceInverse(std::vector<float>& input, int left, int right,
                                     std::vector<int>& indices);
    static void computeDFL(float* tensor, int dfl_len, float* box);
    static float deqntAffineToF32(int8_t qnt, int32_t zp, float scale);
    static int8_t qntF32ToAffine(float f32, int32_t zp, float scale);
#endif
};

} // namespace AISecurityVision

#endif // YOLOV8_RKNN_DETECTOR_H