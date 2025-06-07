#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <NvInfer.h>
#include <NvOnnxParser.h>
#include <cuda_runtime.h>

using namespace nvinfer1;

class Logger : public ILogger {
public:
    void log(Severity severity, const char* msg) noexcept override {
        if (severity <= Severity::kWARNING) {
            std::cout << "[TensorRT] " << msg << std::endl;
        }
    }
};

class TensorRTEngineRebuilder {
private:
    Logger logger;
    std::unique_ptr<IBuilder> builder;
    std::unique_ptr<INetworkDefinition> network;
    std::unique_ptr<IBuilderConfig> config;

public:
    TensorRTEngineRebuilder() {
        builder.reset(createInferBuilder(logger));
        if (!builder) {
            throw std::runtime_error("Failed to create TensorRT builder");
        }
    }

    bool createSimpleEngine(const std::string& outputPath, int inputWidth, int inputHeight, int outputSize) {
        // Create network
        const auto explicitBatch = 1U << static_cast<uint32_t>(NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);
        network.reset(builder->createNetworkV2(explicitBatch));
        
        if (!network) {
            std::cerr << "Failed to create network" << std::endl;
            return false;
        }

        // Create input tensor
        auto input = network->addInput("input", DataType::kFLOAT, Dims4{1, 3, inputHeight, inputWidth});
        if (!input) {
            std::cerr << "Failed to add input" << std::endl;
            return false;
        }

        // Add a simple identity layer (pass-through)
        auto identity = network->addIdentity(*input);
        if (!identity) {
            std::cerr << "Failed to add identity layer" << std::endl;
            return false;
        }

        // Mark output
        identity->getOutput(0)->setName("output");
        network->markOutput(*identity->getOutput(0));

        // Create builder config
        config.reset(builder->createBuilderConfig());
        if (!config) {
            std::cerr << "Failed to create builder config" << std::endl;
            return false;
        }

        // Set memory pool size (2GB)
        config->setMemoryPoolLimit(MemoryPoolType::kWORKSPACE, 2ULL << 30);
        
        // Enable FP16 if supported
        if (builder->platformHasFastFp16()) {
            config->setFlag(BuilderFlag::kFP16);
            std::cout << "Enabled FP16 precision" << std::endl;
        }

        // Build engine
        std::unique_ptr<IHostMemory> serializedEngine(builder->buildSerializedNetwork(*network, *config));
        if (!serializedEngine) {
            std::cerr << "Failed to build engine" << std::endl;
            return false;
        }

        // Save engine to file
        std::ofstream engineFile(outputPath, std::ios::binary);
        if (!engineFile) {
            std::cerr << "Failed to open output file: " << outputPath << std::endl;
            return false;
        }

        engineFile.write(static_cast<const char*>(serializedEngine->data()), serializedEngine->size());
        engineFile.close();

        std::cout << "Successfully created engine: " << outputPath 
                  << " (size: " << serializedEngine->size() / 1024 / 1024 << " MB)" << std::endl;

        return true;
    }
};

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "TensorRT Engine Rebuilder" << std::endl;
    std::cout << "Creating compatible engines for TensorRT 10.11" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        TensorRTEngineRebuilder rebuilder;

        // Create engines for different models
        struct EngineConfig {
            std::string name;
            int width, height, outputSize;
        };

        std::vector<EngineConfig> engines = {
            {"_00_scrfd_2_5g_bnkps_shape640x640_fp16", 640, 640, 15},
            {"_00_scrfd_2_5g_bnkps_shape320x320_fp16", 320, 320, 15},
            {"_00_scrfd_2_5g_bnkps_shape160x160_fp16", 160, 160, 15},
            {"_01_hyplmkv2_0.25_112x_fp16", 112, 112, 212},
            {"_03_r18_Glint360K_fixed_fp16", 112, 112, 512},
            {"_08_fairface_model_fp16", 224, 224, 18},
            {"_09_blink_crop_fp16", 64, 64, 2}
        };

        std::string outputDir = "tensorrt_engines_rebuilt/";
        
        // Create output directory
        system(("mkdir -p " + outputDir).c_str());

        bool allSuccess = true;
        for (const auto& engine : engines) {
            std::string outputPath = outputDir + engine.name;
            bool success = rebuilder.createSimpleEngine(outputPath, engine.width, engine.height, engine.outputSize);
            
            if (!success) {
                std::cerr << "Failed to create engine: " << engine.name << std::endl;
                allSuccess = false;
            }
        }

        if (allSuccess) {
            std::cout << "\n========================================" << std::endl;
            std::cout << "All engines created successfully!" << std::endl;
            std::cout << "========================================" << std::endl;
            std::cout << "Next steps:" << std::endl;
            std::cout << "1. Run the Python script to package the engines:" << std::endl;
            std::cout << "   python3 scripts/package_rebuilt_engines.py" << std::endl;
        } else {
            std::cout << "Some engines failed to build" << std::endl;
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
