/**
 * @file herror.h
 * @brief Error codes and result definitions for InsightFace
 * 
 * This file defines the error codes and result constants used by InsightFace API.
 */

#ifndef INSPIREFACE_HERROR_H
#define INSPIREFACE_HERROR_H

#include "intypedef.h"

// Success code
#define HSUCCEED                    0L      ///< Operation succeeded

// General error codes
#define HERR_INVALID_PARAM         -1L     ///< Invalid parameter
#define HERR_INVALID_OPERATION     -2L     ///< Invalid operation
#define HERR_MEMORY_ALLOCATION     -3L     ///< Memory allocation failed
#define HERR_FILE_NOT_FOUND        -4L     ///< File not found
#define HERR_FILE_READ_ERROR       -5L     ///< File read error
#define HERR_FILE_WRITE_ERROR      -6L     ///< File write error
#define HERR_INVALID_FORMAT        -7L     ///< Invalid format
#define HERR_NOT_INITIALIZED       -8L     ///< Not initialized
#define HERR_ALREADY_INITIALIZED   -9L     ///< Already initialized
#define HERR_TIMEOUT               -10L    ///< Operation timeout
#define HERR_CANCELLED             -11L    ///< Operation cancelled
#define HERR_NOT_SUPPORTED         -12L    ///< Operation not supported
#define HERR_INSUFFICIENT_BUFFER   -13L    ///< Insufficient buffer size
#define HERR_INVALID_STATE         -14L    ///< Invalid state
#define HERR_RESOURCE_BUSY         -15L    ///< Resource is busy

// Model and inference related errors
#define HERR_MODEL_LOAD_FAILED     -100L   ///< Model loading failed
#define HERR_MODEL_INVALID         -101L   ///< Invalid model
#define HERR_INFERENCE_FAILED      -102L   ///< Inference failed
#define HERR_PREPROCESSING_FAILED  -103L   ///< Preprocessing failed
#define HERR_POSTPROCESSING_FAILED -104L   ///< Postprocessing failed
#define HERR_INVALID_INPUT_SIZE    -105L   ///< Invalid input size
#define HERR_INVALID_OUTPUT_SIZE   -106L   ///< Invalid output size

// Face detection and analysis errors
#define HERR_NO_FACE_DETECTED      -200L   ///< No face detected
#define HERR_TOO_MANY_FACES        -201L   ///< Too many faces detected
#define HERR_FACE_TOO_SMALL        -202L   ///< Face too small
#define HERR_FACE_TOO_LARGE        -203L   ///< Face too large
#define HERR_POOR_FACE_QUALITY     -204L   ///< Poor face quality
#define HERR_FACE_ANGLE_TOO_LARGE  -205L   ///< Face angle too large
#define HERR_LANDMARK_FAILED       -206L   ///< Landmark detection failed
#define HERR_ATTRIBUTE_FAILED      -207L   ///< Attribute analysis failed

// Hardware and system errors
#define HERR_GPU_NOT_AVAILABLE     -300L   ///< GPU not available
#define HERR_NPU_NOT_AVAILABLE     -301L   ///< NPU not available
#define HERR_INSUFFICIENT_MEMORY   -302L   ///< Insufficient memory
#define HERR_DEVICE_ERROR          -303L   ///< Device error
#define HERR_DRIVER_ERROR          -304L   ///< Driver error
#define HERR_HARDWARE_ERROR        -305L   ///< Hardware error

// Network and communication errors
#define HERR_NETWORK_ERROR         -400L   ///< Network error
#define HERR_CONNECTION_FAILED     -401L   ///< Connection failed
#define HERR_AUTHENTICATION_FAILED -402L   ///< Authentication failed
#define HERR_PERMISSION_DENIED     -403L   ///< Permission denied
#define HERR_SERVICE_UNAVAILABLE   -404L   ///< Service unavailable

// License and authorization errors
#define HERR_LICENSE_INVALID       -500L   ///< Invalid license
#define HERR_LICENSE_EXPIRED       -501L   ///< License expired
#define HERR_FEATURE_NOT_LICENSED  -502L   ///< Feature not licensed
#define HERR_QUOTA_EXCEEDED        -503L   ///< Quota exceeded

// Note: Log levels are defined in inspireface.h as HFLogLevel enum

/**
 * @brief Check if result indicates success
 * @param result The result code to check
 * @return true if successful, false otherwise
 */
#define HF_IS_SUCCESS(result)      ((result) == HSUCCEED)

/**
 * @brief Check if result indicates failure
 * @param result The result code to check
 * @return true if failed, false otherwise
 */
#define HF_IS_FAILURE(result)      ((result) != HSUCCEED)

/**
 * @brief Get error message string for a result code
 * @param result The result code
 * @return Const string describing the error
 */
const char* HFGetErrorString(HResult result);

// Note: Log functions are declared in inspireface.h

#endif // INSPIREFACE_HERROR_H
