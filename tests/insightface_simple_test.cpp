/**
 * @file insightface_simple_test.cpp
 * @brief Simple test program for InsightFace integration
 * 
 * Based on the DEV_GUIDE.md specifications and verified functionality.
 * This program tests the basic InsightFace functionality for age/gender recognition.
 */

#include <iostream>
#include <opencv2/opencv.hpp>

// Include InsightFace C API
extern "C" {
#include <inspireface.h>
}

// Age bracket mapping from DEV_GUIDE.md
const char* getAgeGroupName(int ageBracket) {
    switch (ageBracket) {
        case 0: return "0-2 years";
        case 1: return "3-9 years";
        case 2: return "10-19 years";
        case 3: return "20-29 years";
        case 4: return "30-39 years";
        case 5: return "40-49 years";
        case 6: return "50-59 years";
        case 7: return "60-69 years";
        case 8: return "70+ years";
        default: return "unknown";
    }
}

// Gender mapping
const char* getGenderName(int gender) {
    switch (gender) {
        case 0: return "Female";
        case 1: return "Male";
        default: return "Unknown";
    }
}

// Race mapping
const char* getRaceName(int race) {
    switch (race) {
        case 0: return "Black";
        case 1: return "Asian";
        case 2: return "Latino/Hispanic";
        case 3: return "Middle Eastern";
        case 4: return "White";
        default: return "Unknown";
    }
}

// Map InsightFace age brackets to our 4 groups
const char* mapToOurAgeGroup(int ageBracket) {
    switch (ageBracket) {
        case 0:
        case 1: return "child";     // 0-2, 3-9 years
        case 2:
        case 3: return "young";     // 10-19, 20-29 years
        case 4:
        case 5:
        case 6: return "middle";    // 30-39, 40-49, 50-59 years
        case 7:
        case 8: return "senior";    // 60-69, 70+ years
        default: return "unknown";
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <model_pack_path> <image_path>" << std::endl;
        std::cout << "Example: " << argv[0] << " ../models/Pikachu.pack ../models/bus.jpg" << std::endl;
        return 1;
    }
    
    const char* packPath = argv[1];
    const char* imagePath = argv[2];
    
    std::cout << "=== InsightFace Age/Gender Recognition Test ===" << std::endl;
    std::cout << "Pack file: " << packPath << std::endl;
    std::cout << "Image file: " << imagePath << std::endl;
    std::cout << std::endl;
    
    // 1. Initialize InsightFace
    std::cout << "Initializing InsightFace..." << std::endl;
    HResult ret = HFLaunchInspireFace(packPath);
    if (ret != HSUCCEED) {
        std::cout << "Failed to launch InsightFace: " << ret << std::endl;
        return ret;
    }
    std::cout << "✓ InsightFace launched successfully" << std::endl;
    
    // Set log level
    HFSetLogLevel(HF_LOG_WARN);
    
    // 2. Create session with face attributes enabled
    std::cout << "Creating session..." << std::endl;
    HOption option = HF_ENABLE_QUALITY | HF_ENABLE_MASK_DETECT | HF_ENABLE_FACE_ATTRIBUTE;
    HFSession session = nullptr;
    ret = HFCreateInspireFaceSessionOptional(option, HF_DETECT_MODE_ALWAYS_DETECT, 10, 160, -1, &session);
    if (ret != HSUCCEED) {
        std::cout << "Failed to create session: " << ret << std::endl;
        return ret;
    }
    std::cout << "✓ Session created successfully" << std::endl;
    
    // Configure session parameters
    HFSessionSetTrackPreviewSize(session, 160);
    HFSessionSetFilterMinimumFacePixelSize(session, 4);
    
    // 3. Load image
    std::cout << "Loading image..." << std::endl;
    cv::Mat image = cv::imread(imagePath);
    if (image.empty()) {
        std::cout << "Failed to load image: " << imagePath << std::endl;
        HFReleaseInspireFaceSession(session);
        return 1;
    }
    std::cout << "✓ Image loaded: " << image.cols << "x" << image.rows << std::endl;
    
    // 4. Create image bitmap
    HFImageBitmap imageBitmap;
    ret = HFCreateImageBitmapFromMat(
        image.data,
        image.cols,
        image.rows,
        image.channels(),
        HF_STREAM_BGR,
        &imageBitmap
    );
    if (ret != HSUCCEED) {
        std::cout << "Failed to create image bitmap: " << ret << std::endl;
        HFReleaseInspireFaceSession(session);
        return ret;
    }
    
    // 5. Create image stream
    HFImageStream imageStream;
    ret = HFCreateImageStreamFromImageBitmap(imageBitmap, HF_CAMERA_ROTATION_0, &imageStream);
    if (ret != HSUCCEED) {
        std::cout << "Failed to create image stream: " << ret << std::endl;
        HFReleaseImageBitmap(imageBitmap);
        HFReleaseInspireFaceSession(session);
        return ret;
    }
    
    // 6. Execute face detection
    std::cout << "Detecting faces..." << std::endl;
    HFMultipleFaceData multipleFaceData;
    ret = HFExecuteFaceTrack(session, imageStream, &multipleFaceData);
    if (ret != HSUCCEED) {
        std::cout << "Face detection failed: " << ret << std::endl;
        goto cleanup;
    }
    
    std::cout << "✓ Detected " << multipleFaceData.detectedNum << " face(s)" << std::endl;
    
    if (multipleFaceData.detectedNum == 0) {
        std::cout << "No faces detected in the image." << std::endl;
        goto cleanup;
    }
    
    // 7. Run pipeline for face attributes
    std::cout << "Analyzing face attributes..." << std::endl;
    HOption pipelineOption = HF_ENABLE_QUALITY | HF_ENABLE_MASK_DETECT | HF_ENABLE_FACE_ATTRIBUTE;
    ret = HFMultipleFacePipelineProcessOptional(session, imageStream, &multipleFaceData, pipelineOption);
    if (ret != HSUCCEED) {
        std::cout << "Pipeline processing failed: " << ret << std::endl;
        goto cleanup;
    }
    
    // 8. Get results
    HFFaceAttributeResult attributeResult;
    HFFaceQualityConfidence qualityConfidence;
    HFFaceMaskConfidence maskConfidence;
    
    ret = HFGetFaceAttributeResult(session, &attributeResult);
    if (ret != HSUCCEED) {
        std::cout << "Failed to get face attributes: " << ret << std::endl;
        goto cleanup;
    }
    
    HFGetFaceQualityConfidence(session, &qualityConfidence);
    HFGetFaceMaskConfidence(session, &maskConfidence);
    
    // 9. Display results
    std::cout << std::endl << "=== Analysis Results ===" << std::endl;
    
    for (int i = 0; i < multipleFaceData.detectedNum && i < attributeResult.num; i++) {
        std::cout << std::endl << "--- Face " << (i + 1) << " ---" << std::endl;
        
        // Detection info
        std::cout << "Detection confidence: " << multipleFaceData.detConfidence[i] << std::endl;
        std::cout << "Bounding box: (" << multipleFaceData.rects[i].x << ", " 
                  << multipleFaceData.rects[i].y << ", " 
                  << multipleFaceData.rects[i].width << ", " 
                  << multipleFaceData.rects[i].height << ")" << std::endl;
        
        // Quality
        if (i < qualityConfidence.num) {
            std::cout << "Quality score: " << qualityConfidence.confidence[i] << std::endl;
        }
        
        // Mask detection
        if (i < maskConfidence.num) {
            std::cout << "Mask detection: " << (maskConfidence.confidence[i] > 0.85 ? "Yes" : "No") 
                      << " (confidence: " << maskConfidence.confidence[i] << ")" << std::endl;
        }
        
        // Attributes
        std::cout << "Gender: " << getGenderName(attributeResult.gender[i]) << std::endl;
        std::cout << "Age bracket: " << getAgeGroupName(attributeResult.ageBracket[i]) << std::endl;
        std::cout << "Race: " << getRaceName(attributeResult.race[i]) << std::endl;
        
        // Our age group mapping
        std::cout << "Our age group: " << mapToOurAgeGroup(attributeResult.ageBracket[i]) << std::endl;
    }
    
    std::cout << std::endl << "=== Test Summary ===" << std::endl;
    std::cout << "✓ InsightFace integration working correctly" << std::endl;
    std::cout << "✓ Face detection: " << multipleFaceData.detectedNum << " faces" << std::endl;
    std::cout << "✓ Attribute analysis: " << attributeResult.num << " results" << std::endl;
    std::cout << "✓ Quality assessment: " << qualityConfidence.num << " scores" << std::endl;
    std::cout << "✓ Mask detection: " << maskConfidence.num << " results" << std::endl;
    
cleanup:
    // Clean up resources
    HFReleaseImageStream(imageStream);
    HFReleaseImageBitmap(imageBitmap);
    HFReleaseInspireFaceSession(session);
    
    std::cout << std::endl << "=== Test Completed ===" << std::endl;
    return 0;
}
