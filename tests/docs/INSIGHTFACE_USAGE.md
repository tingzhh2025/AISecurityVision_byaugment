# InsightFace Integration Usage Guide

## Quick Start

### 1. Test InsightFace functionality

```bash
# Build and test InsightFace
cd tests
./build_insightface_test.sh

# Run test with model pack and test image
./insightface_simple_test ../models/Pikachu.pack test_image.jpg
```

### 2. Integration with AI Security Vision System

The system now supports InsightFace for enhanced face analysis:

- **Age Recognition**: 9 age brackets (0-2, 3-9, 10-19, 20-29, 30-39, 40-49, 50-59, 60-69, 70+)
- **Gender Recognition**: Male/Female
- **Race Recognition**: Black, Asian, Latino/Hispanic, Middle Eastern, White
- **Quality Assessment**: Face quality scoring
- **Mask Detection**: Automatic mask detection

### 3. Model Pack

The model pack `Pikachu.pack` (copied from Gundam_RK3588) contains:
- Face detection models
- Age/gender recognition models
- Quality assessment models
- Mask detection models

### 4. API Usage

```cpp
#include "AgeGenderAnalyzer.h"

// Initialize with InsightFace
AgeGenderAnalyzer analyzer;
analyzer.initialize("models/Pikachu.pack");

// Analyze person crops
std::vector<PersonDetection> persons = PersonFilter::filterPersons(detections, frame);
auto attributes = analyzer.analyze(persons);

// Access enhanced attributes
for (const auto& attr : attributes) {
    std::cout << "Gender: " << attr.gender << std::endl;
    std::cout << "Age: " << attr.age_group << std::endl;
    std::cout << "Race: " << attr.race << std::endl;
    std::cout << "Quality: " << attr.quality_score << std::endl;
    std::cout << "Mask: " << (attr.has_mask ? "Yes" : "No") << std::endl;
}
```

### 5. Performance

Expected performance on RK3588:
- Face detection: ~10-20ms per image
- Attribute analysis: ~5-15ms per face
- Total processing: ~15-35ms per person

### 6. Troubleshooting

If you encounter issues:

1. **Library not found**: Ensure InsightFace is built in the expected location
2. **Model pack missing**: Check that Pikachu.pack exists in models/ directory
3. **Runtime errors**: Verify RPATH is set correctly for shared libraries

### 7. Migration from RKNN

The enhanced PersonAttributes structure is backward compatible:
- Existing `gender` and `age_group` fields work as before
- New fields (`race`, `quality_score`, `has_mask`) provide additional information
- All existing code continues to work without modification
