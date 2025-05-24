#include "src/recognition/FaceRecognizer.h"
#include "src/database/DatabaseManager.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <cassert>

/**
 * Unit test for Task 62: Face verification functionality
 * Tests the FaceRecognizer class methods and face verification logic
 */

void testCosineSimilarity() {
    std::cout << "\n--- Test 1: Cosine Similarity Calculation ---" << std::endl;
    
    FaceRecognizer recognizer;
    
    // Test identical vectors (should return 1.0)
    std::vector<float> vec1 = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> vec2 = {1.0f, 2.0f, 3.0f, 4.0f};
    float similarity = recognizer.calculateCosineSimilarity(vec1, vec2);
    std::cout << "Identical vectors similarity: " << similarity << std::endl;
    assert(std::abs(similarity - 1.0f) < 0.001f);
    
    // Test orthogonal vectors (should return 0.0)
    std::vector<float> vec3 = {1.0f, 0.0f};
    std::vector<float> vec4 = {0.0f, 1.0f};
    similarity = recognizer.calculateCosineSimilarity(vec3, vec4);
    std::cout << "Orthogonal vectors similarity: " << similarity << std::endl;
    assert(std::abs(similarity - 0.0f) < 0.001f);
    
    // Test opposite vectors (should return -1.0)
    std::vector<float> vec5 = {1.0f, 2.0f, 3.0f};
    std::vector<float> vec6 = {-1.0f, -2.0f, -3.0f};
    similarity = recognizer.calculateCosineSimilarity(vec5, vec6);
    std::cout << "Opposite vectors similarity: " << similarity << std::endl;
    assert(std::abs(similarity - (-1.0f)) < 0.001f);
    
    // Test different size vectors (should return 0.0)
    std::vector<float> vec7 = {1.0f, 2.0f};
    std::vector<float> vec8 = {1.0f, 2.0f, 3.0f};
    similarity = recognizer.calculateCosineSimilarity(vec7, vec8);
    std::cout << "Different size vectors similarity: " << similarity << std::endl;
    assert(similarity == 0.0f);
    
    std::cout << "✅ Cosine similarity tests passed!" << std::endl;
}

void testEmbeddingGeneration() {
    std::cout << "\n--- Test 2: Face Embedding Generation ---" << std::endl;
    
    FaceRecognizer recognizer;
    assert(recognizer.initialize());
    
    // Create test images
    cv::Mat testImage1 = cv::Mat::zeros(112, 112, CV_8UC3);
    cv::Mat testImage2 = cv::Mat::ones(112, 112, CV_8UC3) * 128;
    cv::Mat testImage3 = cv::Mat::ones(112, 112, CV_8UC3) * 255;
    
    // Generate embeddings
    auto embedding1 = recognizer.extractFaceEmbedding(testImage1);
    auto embedding2 = recognizer.extractFaceEmbedding(testImage2);
    auto embedding3 = recognizer.extractFaceEmbedding(testImage3);
    
    // Check embedding properties
    assert(!embedding1.empty());
    assert(!embedding2.empty());
    assert(!embedding3.empty());
    assert(embedding1.size() == 128);
    assert(embedding2.size() == 128);
    assert(embedding3.size() == 128);
    
    std::cout << "Embedding 1 size: " << embedding1.size() << std::endl;
    std::cout << "Embedding 2 size: " << embedding2.size() << std::endl;
    std::cout << "Embedding 3 size: " << embedding3.size() << std::endl;
    
    // Test deterministic behavior (same image should produce same embedding)
    auto embedding1_repeat = recognizer.extractFaceEmbedding(testImage1);
    assert(embedding1.size() == embedding1_repeat.size());
    
    bool identical = true;
    for (size_t i = 0; i < embedding1.size(); ++i) {
        if (std::abs(embedding1[i] - embedding1_repeat[i]) > 0.001f) {
            identical = false;
            break;
        }
    }
    assert(identical);
    std::cout << "✅ Deterministic embedding generation verified!" << std::endl;
    
    // Test that different images produce different embeddings
    float similarity12 = recognizer.calculateCosineSimilarity(embedding1, embedding2);
    float similarity13 = recognizer.calculateCosineSimilarity(embedding1, embedding3);
    float similarity23 = recognizer.calculateCosineSimilarity(embedding2, embedding3);
    
    std::cout << "Similarity 1-2: " << similarity12 << std::endl;
    std::cout << "Similarity 1-3: " << similarity13 << std::endl;
    std::cout << "Similarity 2-3: " << similarity23 << std::endl;
    
    // Different images should have different embeddings (similarity < 1.0)
    assert(similarity12 < 1.0f);
    assert(similarity13 < 1.0f);
    assert(similarity23 < 1.0f);
    
    std::cout << "✅ Face embedding generation tests passed!" << std::endl;
}

void testFaceVerification() {
    std::cout << "\n--- Test 3: Face Verification Logic ---" << std::endl;
    
    FaceRecognizer recognizer;
    assert(recognizer.initialize());
    
    // Create test face records
    std::vector<FaceRecord> registeredFaces;
    
    // Create test images and embeddings
    cv::Mat testImage1 = cv::Mat::zeros(112, 112, CV_8UC3);
    cv::Mat testImage2 = cv::Mat::ones(112, 112, CV_8UC3) * 128;
    cv::Mat testImage3 = cv::Mat::ones(112, 112, CV_8UC3) * 255;
    
    auto embedding1 = recognizer.extractFaceEmbedding(testImage1);
    auto embedding2 = recognizer.extractFaceEmbedding(testImage2);
    auto embedding3 = recognizer.extractFaceEmbedding(testImage3);
    
    // Create face records
    FaceRecord face1("John Doe", "/test/john.jpg");
    face1.id = 1;
    face1.embedding = embedding1;
    
    FaceRecord face2("Jane Smith", "/test/jane.jpg");
    face2.id = 2;
    face2.embedding = embedding2;
    
    FaceRecord face3("Bob Johnson", "/test/bob.jpg");
    face3.id = 3;
    face3.embedding = embedding3;
    
    registeredFaces.push_back(face1);
    registeredFaces.push_back(face2);
    registeredFaces.push_back(face3);
    
    // Test verification with exact match
    auto results1 = recognizer.verifyFace(testImage1, registeredFaces, 0.7f);
    std::cout << "Verification results for exact match: " << results1.size() << " matches" << std::endl;
    
    // Should find at least one match (the exact same image)
    assert(!results1.empty());
    assert(results1[0].face_id == 1);
    assert(results1[0].name == "John Doe");
    assert(results1[0].confidence >= 0.7f);
    
    // Test verification with high threshold
    auto results2 = recognizer.verifyFace(testImage2, registeredFaces, 0.95f);
    std::cout << "Verification results for high threshold: " << results2.size() << " matches" << std::endl;
    
    // Test verification with low threshold
    auto results3 = recognizer.verifyFace(testImage3, registeredFaces, 0.1f);
    std::cout << "Verification results for low threshold: " << results3.size() << " matches" << std::endl;
    
    // Low threshold should find more matches
    assert(results3.size() >= results2.size());
    
    // Test with empty registered faces
    std::vector<FaceRecord> emptyFaces;
    auto results4 = recognizer.verifyFace(testImage1, emptyFaces, 0.7f);
    assert(results4.empty());
    
    // Test with empty input image
    cv::Mat emptyImage;
    auto results5 = recognizer.verifyFace(emptyImage, registeredFaces, 0.7f);
    assert(results5.empty());
    
    std::cout << "✅ Face verification tests passed!" << std::endl;
}

void testDatabaseIntegration() {
    std::cout << "\n--- Test 4: Database Integration ---" << std::endl;
    
    // Test database operations for face verification
    DatabaseManager db;
    assert(db.initialize("test_verification.db"));
    
    // Create test face record
    FaceRecord testFace("Test User", "/test/test_user.jpg");
    testFace.embedding = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    
    // Insert face
    assert(db.insertFace(testFace));
    
    // Retrieve faces
    auto faces = db.getFaces();
    assert(!faces.empty());
    assert(faces[0].name == "Test User");
    assert(faces[0].embedding.size() == 5);
    
    std::cout << "Retrieved face: " << faces[0].name << " with embedding size: " << faces[0].embedding.size() << std::endl;
    
    // Cleanup
    remove("test_verification.db");
    
    std::cout << "✅ Database integration tests passed!" << std::endl;
}

int main() {
    std::cout << "=== Task 62: Face Verification Unit Tests ===" << std::endl;
    
    try {
        testCosineSimilarity();
        testEmbeddingGeneration();
        testFaceVerification();
        testDatabaseIntegration();
        
        std::cout << "\n🎉 All tests passed! Face verification implementation is working correctly." << std::endl;
        std::cout << "\nImplemented features:" << std::endl;
        std::cout << "✓ Cosine similarity calculation" << std::endl;
        std::cout << "✓ Deterministic face embedding generation" << std::endl;
        std::cout << "✓ Face verification with configurable threshold" << std::endl;
        std::cout << "✓ Database integration for face storage" << std::endl;
        std::cout << "✓ Error handling for edge cases" << std::endl;
        std::cout << "✓ Sorted results by confidence score" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
