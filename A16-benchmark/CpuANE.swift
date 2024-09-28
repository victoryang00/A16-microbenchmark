//
//  CpuANE.swift
//  A16-benchmark
//
//  Created by Yiwei Yang on 12/22/23.
//

import Foundation
import CoreML

class MNISTInput: MLFeatureProvider {
    var featureNames: Set<String> {
        return ["image", "image2"]
    }
    
    func featureValue(for featureName: String) -> MLFeatureValue? {
        if featureName == "image" || featureName == "image2" {
            let tokenIDMultiArray = try? MLMultiArray(shape: [64], dataType: .float32)
            tokenIDMultiArray?[0] = NSNumber(value: 1337)
            return MLFeatureValue(multiArray: tokenIDMultiArray!)
        }
        return nil
    }
}

class LatencyTester {
    let model: MLModel
    let cpuModel: MLModel
    let aneModel: MLModel
    
    init(modelURL: URL) throws {
        let compiledUrl = try MLModel.compileModel(at: modelURL)
        
        // CPU configuration
        let cpuConfig = MLModelConfiguration()
        cpuConfig.computeUnits = .cpuOnly
        self.cpuModel = try MLModel(contentsOf: compiledUrl, configuration: cpuConfig)
        
        // ANE configuration
        let aneConfig = MLModelConfiguration()
        aneConfig.computeUnits = .neuralEngine
        self.aneModel = try MLModel(contentsOf: compiledUrl, configuration: aneConfig)
        
        // Default model (will use ANE if available)
        let defaultConfig = MLModelConfiguration()
        defaultConfig.computeUnits = .all
        self.model = try MLModel(contentsOf: compiledUrl, configuration: defaultConfig)
    }
    
    func runLatencyTest(iterations: Int) {
        let input = MNISTInput()
        
        // Warm-up runs
        _ = try? cpuModel.prediction(from: input)
        _ = try? aneModel.prediction(from: input)
        
        var cpuTotalTime = 0.0
        var aneTotalTime = 0.0
        
        for _ in 0..<iterations {
            // CPU run
            let cpuStart = CACurrentMediaTime()
            _ = try? cpuModel.prediction(from: input)
            let cpuEnd = CACurrentMediaTime()
            cpuTotalTime += cpuEnd - cpuStart
            
            // ANE run
            let aneStart = CACurrentMediaTime()
            _ = try? aneModel.prediction(from: input)
            let aneEnd = CACurrentMediaTime()
            aneTotalTime += aneEnd - aneStart
        }
        
        let cpuAverageTime = cpuTotalTime / Double(iterations)
        let aneAverageTime = aneTotalTime / Double(iterations)
        
        print("CPU average time: \(cpuAverageTime * 1000) ms")
        print("ANE average time: \(aneAverageTime * 1000) ms")
        print("Latency difference: \(abs(cpuAverageTime - aneAverageTime) * 1000) ms")
    }
}
