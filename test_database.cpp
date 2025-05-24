#include "src/database/DatabaseManager.h"
#include <iostream>

int main() {
    std::cout << "=== Database Test ===" << std::endl;
    
    // Initialize database
    DatabaseManager db;
    if (!db.initialize("test.db")) {
        std::cerr << "Failed to initialize database: " << db.getErrorMessage() << std::endl;
        return 1;
    }
    
    std::cout << "Database initialized successfully" << std::endl;
    
    // Test event insertion
    EventRecord event("test_camera_01", "intrusion", "/recordings/test_event.mp4", 0.85);
    event.metadata = "Test intrusion event";
    
    if (db.insertEvent(event)) {
        std::cout << "Event inserted successfully" << std::endl;
    } else {
        std::cerr << "Failed to insert event: " << db.getErrorMessage() << std::endl;
        return 1;
    }
    
    // Test event retrieval
    auto events = db.getEvents("test_camera_01");
    std::cout << "Retrieved " << events.size() << " events" << std::endl;
    
    for (const auto& e : events) {
        std::cout << "Event: " << e.event_type 
                  << ", Camera: " << e.camera_id
                  << ", Timestamp: " << e.timestamp
                  << ", Video: " << e.video_path
                  << ", Confidence: " << e.confidence << std::endl;
    }
    
    // Test face insertion
    FaceRecord face("John Doe", "/faces/john_doe.jpg");
    face.embedding = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f}; // Sample embedding
    
    if (db.insertFace(face)) {
        std::cout << "Face inserted successfully" << std::endl;
    } else {
        std::cerr << "Failed to insert face: " << db.getErrorMessage() << std::endl;
    }
    
    // Test face retrieval
    auto faces = db.getFaces();
    std::cout << "Retrieved " << faces.size() << " faces" << std::endl;
    
    for (const auto& f : faces) {
        std::cout << "Face: " << f.name 
                  << ", Image: " << f.image_path
                  << ", Embedding size: " << f.embedding.size() << std::endl;
    }
    
    std::cout << "Database test completed successfully!" << std::endl;
    return 0;
}
