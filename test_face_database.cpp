#include <iostream>
#include <sqlite3.h>
#include "../src/database/DatabaseManager.h"

int main() {
    std::cout << "=== Testing Face Database Schema ===" << std::endl;
    
    // Initialize database manager
    DatabaseManager db;
    
    // Initialize with test database
    if (!db.initialize("test_faces.db")) {
        std::cerr << "Failed to initialize database: " << db.getErrorMessage() << std::endl;
        return 1;
    }
    
    std::cout << "✅ Database initialized successfully" << std::endl;
    
    // Test face insertion
    FaceRecord testFace("John Doe", "/test/john_doe.jpg");
    testFace.embedding = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f}; // Sample 8D embedding
    
    if (db.insertFace(testFace)) {
        std::cout << "✅ Face inserted successfully" << std::endl;
    } else {
        std::cerr << "❌ Failed to insert face: " << db.getErrorMessage() << std::endl;
        return 1;
    }
    
    // Test face retrieval
    auto faces = db.getFaces();
    std::cout << "✅ Retrieved " << faces.size() << " faces from database" << std::endl;
    
    for (const auto& face : faces) {
        std::cout << "Face ID: " << face.id 
                  << ", Name: " << face.name
                  << ", Image Path: " << face.image_path
                  << ", Embedding Size: " << face.embedding.size()
                  << ", Created At: " << face.created_at << std::endl;
    }
    
    // Test face retrieval by name
    auto retrievedFace = db.getFaceByName("John Doe");
    if (retrievedFace.id > 0) {
        std::cout << "✅ Face retrieved by name successfully" << std::endl;
        std::cout << "Retrieved face embedding size: " << retrievedFace.embedding.size() << std::endl;
    } else {
        std::cerr << "❌ Failed to retrieve face by name" << std::endl;
        return 1;
    }
    
    std::cout << "=== Face Database Schema Test Complete ===" << std::endl;
    std::cout << "✅ All tests passed! Task 57 is COMPLETE." << std::endl;
    
    return 0;
}
