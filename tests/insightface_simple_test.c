#include <stdio.h>
#include <stdlib.h>
#include <inspireface.h>

int main(int argc, char* argv[]) {
    HResult ret;
    const char* packPath;
    const char* sourcePath;
    int rotation = 0;
    HFRotation rotation_enum = HF_CAMERA_ROTATION_0;
    HOption option;
    HFDetectMode detMode;
    HInt32 maxDetectNum;
    HInt32 detectPixelLevel;
    HFSession session;
    HFImageBitmap image;
    HFImageStream imageHandle;
    HFMultipleFaceData multipleFaceData;
    int faceNum;
    HFImageBitmap drawImage;
    HFImageBitmapData data;
    int index;
    HFFaceAttributeResult attributeResult;
    HOption pipelineOption;

    /* Check whether the number of parameters is correct */
    if (argc < 3 || argc > 4) {
        HFLogPrint(HF_LOG_ERROR, "Usage: %s <pack_path> <source_path> [rotation]", argv[0]);
        return 1;
    }

    packPath = argv[1];
    sourcePath = argv[2];

    /* If rotation is provided, check and set the value */
    if (argc == 4) {
        rotation = atoi(argv[3]);
        if (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270) {
            HFLogPrint(HF_LOG_ERROR, "Invalid rotation value. Allowed values are 0, 90, 180, 270.");
            return 1;
        }
    }

    /* Set rotation based on input parameter */
    switch (rotation) {
        case 90:
            rotation_enum = HF_CAMERA_ROTATION_90;
            break;
        case 180:
            rotation_enum = HF_CAMERA_ROTATION_180;
            break;
        case 270:
            rotation_enum = HF_CAMERA_ROTATION_270;
            break;
        case 0:
        default:
            rotation_enum = HF_CAMERA_ROTATION_0;
            break;
    }

    HFLogPrint(HF_LOG_INFO, "=== Face Age and Gender Detection Sample ===");
    HFLogPrint(HF_LOG_INFO, "Pack file Path: %s", packPath);
    HFLogPrint(HF_LOG_INFO, "Source file Path: %s", sourcePath);
    HFLogPrint(HF_LOG_INFO, "Rotation: %d", rotation);

    HFSetLogLevel(HF_LOG_INFO);

    /* The resource file must be loaded before it can be used */
    ret = HFLaunchInspireFace(packPath);
    if (ret != HSUCCEED) {
        HFLogPrint(HF_LOG_ERROR, "Load Resource error: %d", ret);
        return ret;
    }

    /* Enable face attribute detection along with other features */
    option = HF_ENABLE_FACE_ATTRIBUTE | HF_ENABLE_QUALITY | HF_ENABLE_MASK_DETECT;
    
    /* Non-video or frame sequence mode uses IMAGE-MODE, which is always face detection without tracking */
    detMode = HF_DETECT_MODE_ALWAYS_DETECT;
    
    /* Maximum number of faces detected */
    maxDetectNum = 20;
    
    /* Face detection image input level */
    detectPixelLevel = 160;
    
    /* Handle of the current face SDK algorithm context */
    session = NULL;
    ret = HFCreateInspireFaceSessionOptional(option, detMode, maxDetectNum, detectPixelLevel, -1, &session);
    if (ret != HSUCCEED) {
        HFLogPrint(HF_LOG_ERROR, "Create FaceContext error: %d", ret);
        return ret;
    }

    HFSessionSetTrackPreviewSize(session, detectPixelLevel);
    HFSessionSetFilterMinimumFacePixelSize(session, 4);

    /* Load a image */
    ret = HFCreateImageBitmapFromFilePath(sourcePath, 3, &image);
    if (ret != HSUCCEED) {
        HFLogPrint(HF_LOG_ERROR, "The source entered is not a picture or read error.");
        return ret;
    }
    
    /* Prepare an image parameter structure for configuration */
    ret = HFCreateImageStreamFromImageBitmap(image, rotation_enum, &imageHandle);
    if (ret != HSUCCEED) {
        HFLogPrint(HF_LOG_ERROR, "Create ImageStream error: %d", ret);
        return ret;
    }

    /* Execute face detection */
    ret = HFExecuteFaceTrack(session, imageHandle, &multipleFaceData);
    if (ret != HSUCCEED) {
        HFLogPrint(HF_LOG_ERROR, "Execute HFExecuteFaceTrack error: %d", ret);
        return ret;
    }

    /* Print the number of faces detected */
    faceNum = multipleFaceData.detectedNum;
    HFLogPrint(HF_LOG_INFO, "Number of faces detected: %d", faceNum);

    if (faceNum == 0) {
        HFLogPrint(HF_LOG_WARN, "No faces detected in the image.");
        goto cleanup;
    }

    /* Copy a new image to draw */
    ret = HFImageBitmapCopy(image, &drawImage);
    if (ret != HSUCCEED) {
        HFLogPrint(HF_LOG_ERROR, "Copy ImageBitmap error: %d", ret);
        return ret;
    }

    /* Run pipeline function to get face attributes */
    pipelineOption = HF_ENABLE_FACE_ATTRIBUTE | HF_ENABLE_QUALITY | HF_ENABLE_MASK_DETECT;
    ret = HFMultipleFacePipelineProcessOptional(session, imageHandle, &multipleFaceData, pipelineOption);
    if (ret != HSUCCEED) {
        HFLogPrint(HF_LOG_ERROR, "Execute Pipeline error: %d", ret);
        return ret;
    }

    /* Get face attribute results from the pipeline cache */
    ret = HFGetFaceAttributeResult(session, &attributeResult);
    if (ret != HSUCCEED) {
        HFLogPrint(HF_LOG_ERROR, "Get face attribute result error: %d", ret);
        return -1;
    }

    HFLogPrint(HF_LOG_INFO, "Face attribute results for %d faces:", attributeResult.num);

    /* Process each detected face */
    for (index = 0; index < faceNum && index < attributeResult.num; ++index) {
        const char* genderStr;
        const char* ageStr;
        const char* raceStr;
        
        HFLogPrint(HF_LOG_INFO, "========================================");
        HFLogPrint(HF_LOG_INFO, "Face %d:", index + 1);
        HFLogPrint(HF_LOG_INFO, "Detection confidence: %.3f", multipleFaceData.detConfidence[index]);
        
        /* Draw face rectangle */
        HFImageBitmapDrawRect(drawImage, multipleFaceData.rects[index], (HColor){0, 255, 0}, 3);
        
        /* Gender interpretation */
        switch (attributeResult.gender[index]) {
            case 0:
                genderStr = "Female";
                break;
            case 1:
                genderStr = "Male";
                break;
            default:
                genderStr = "Unknown";
                break;
        }
        
        /* Age bracket interpretation */
        switch (attributeResult.ageBracket[index]) {
            case 0:
                ageStr = "0-2 years";
                break;
            case 1:
                ageStr = "3-9 years";
                break;
            case 2:
                ageStr = "10-19 years";
                break;
            case 3:
                ageStr = "20-29 years";
                break;
            case 4:
                ageStr = "30-39 years";
                break;
            case 5:
                ageStr = "40-49 years";
                break;
            case 6:
                ageStr = "50-59 years";
                break;
            case 7:
                ageStr = "60-69 years";
                break;
            case 8:
                ageStr = "70+ years";
                break;
            default:
                ageStr = "Unknown";
                break;
        }
        
        /* Race interpretation */
        switch (attributeResult.race[index]) {
            case 0:
                raceStr = "Black";
                break;
            case 1:
                raceStr = "Asian";
                break;
            case 2:
                raceStr = "Latino/Hispanic";
                break;
            case 3:
                raceStr = "Middle Eastern";
                break;
            case 4:
                raceStr = "White";
                break;
            default:
                raceStr = "Unknown";
                break;
        }
        
        HFLogPrint(HF_LOG_INFO, "Gender: %s (code: %d)", genderStr, attributeResult.gender[index]);
        HFLogPrint(HF_LOG_INFO, "Age bracket: %s (code: %d)", ageStr, attributeResult.ageBracket[index]);
        HFLogPrint(HF_LOG_INFO, "Race: %s (code: %d)", raceStr, attributeResult.race[index]);
    }

    /* Save the result image with face rectangles */
    HFImageBitmapWriteToFile(drawImage, "face_age_gender_result.jpg");
    HFLogPrint(HF_LOG_INFO, "Result image saved to: face_age_gender_result.jpg");

cleanup:
    /* Clean up resources */
    if (imageHandle) {
        ret = HFReleaseImageStream(imageHandle);
        if (ret != HSUCCEED) {
            HFLogPrint(HF_LOG_ERROR, "Release image stream error: %d", ret);
        }
    }
    
    if (session) {
        ret = HFReleaseInspireFaceSession(session);
        if (ret != HSUCCEED) {
            HFLogPrint(HF_LOG_ERROR, "Release session error: %d", ret);
        }
    }

    if (image) {
        ret = HFReleaseImageBitmap(image);
        if (ret != HSUCCEED) {
            HFLogPrint(HF_LOG_ERROR, "Release image bitmap error: %d", ret);
        }
    }

    if (drawImage) {
        ret = HFReleaseImageBitmap(drawImage);
        if (ret != HSUCCEED) {
            HFLogPrint(HF_LOG_ERROR, "Release draw image bitmap error: %d", ret);
        }
    }

    HFLogPrint(HF_LOG_INFO, "=== Face Age and Gender Detection Completed ===");
    HFDeBugShowResourceStatistics();

    return 0;
}
