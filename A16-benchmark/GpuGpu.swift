import Metal
import Foundation

let metalShaderSource = """
#include <metal_stdlib>
using namespace metal;

kernel void testSync(device atomic_int* readLoc [[buffer(0)]],
                     device atomic_int* writeLoc [[buffer(1)]],
                     device uint64_t& iterations [[buffer(2)]],
                     device uint64_t* timestamps [[buffer(3)]],
                     uint thread_position_in_grid [[thread_position_in_grid]]) {
    uint64_t start = mach_absolute_time();
    while (iterations > 0) {
        while (atomic_exchange_explicit(readLoc, 0, memory_order_acquire) == 0) {
            // Spin wait
        }
        atomic_store_explicit(writeLoc, 1, memory_order_release);
        iterations--;
    }
    uint64_t end = mach_absolute_time();
    timestamps[thread_position_in_grid * 2] = start;
    timestamps[thread_position_in_grid * 2 + 1] = end;
}
"""

// MARK: - Main Program

class GPULatencyTest {
    let device: MTLDevice
    let commandQueue: MTLCommandQueue
    let pipelineState: MTLComputePipelineState
    
    init() throws {
        guard let device = MTLCreateSystemDefaultDevice() else {
            throw NSError(domain: "GPULatencyTest", code: 1, userInfo: [NSLocalizedDescriptionKey: "Failed to create Metal device"])
        }
        self.device = device
        
        guard let commandQueue = device.makeCommandQueue() else {
            throw NSError(domain: "GPULatencyTest", code: 2, userInfo: [NSLocalizedDescriptionKey: "Failed to create command queue"])
        }
        self.commandQueue = commandQueue
        
        let library = try device.makeLibrary(source: metalShaderSource, options: nil)
        guard let function = library.makeFunction(name: "testSync") else {
            throw NSError(domain: "GPULatencyTest", code: 3, userInfo: [NSLocalizedDescriptionKey: "Failed to create pipeline function"])
        }
        
        self.pipelineState = try device.makeComputePipelineState(function: function)
    }
    
    func runTest(iterations: inout UInt64) ->  Double {
        let buffer1 = device.makeBuffer(length: MemoryLayout<Int32>.size, options: .storageModeShared)!
        let buffer2 = device.makeBuffer(length: MemoryLayout<Int32>.size, options: .storageModeShared)!
        let iterationsBuffer = device.makeBuffer(bytes: &iterations, length: MemoryLayout<UInt64>.size, options: .storageModeShared)!
        let timestampsBuffer = device.makeBuffer(length: MemoryLayout<UInt64>.size * 4, options: .storageModeShared)!
        
        let commandBuffer = commandQueue.makeCommandBuffer()!
        
        let startTime = DispatchTime.now()
        
        let encoder1 = commandBuffer.makeComputeCommandEncoder()!
        encoder1.setComputePipelineState(pipelineState)
        encoder1.setBuffer(buffer1, offset: 0, index: 0)
        encoder1.setBuffer(buffer2, offset: 0, index: 1)
        encoder1.setBuffer(iterationsBuffer, offset: 0, index: 2)
        encoder1.setBuffer(timestampsBuffer, offset: 0, index: 3)
        encoder1.dispatchThreads(MTLSizeMake(1, 1, 1), threadsPerThreadgroup: MTLSizeMake(1, 1, 1))
        encoder1.endEncoding()
        
        let encoder2 = commandBuffer.makeComputeCommandEncoder()!
        encoder2.setComputePipelineState(pipelineState)
        encoder2.setBuffer(buffer2, offset: 0, index: 0)
        encoder2.setBuffer(buffer1, offset: 0, index: 1)
        encoder2.setBuffer(iterationsBuffer, offset: 0, index: 2)
        encoder2.setBuffer(timestampsBuffer, offset: 0, index: 3)
        encoder2.dispatchThreads(MTLSizeMake(1, 1, 1), threadsPerThreadgroup: MTLSizeMake(1, 1, 1))
        encoder2.endEncoding()
        
        commandBuffer.commit()
        commandBuffer.waitUntilCompleted()
        
        let endTime = DispatchTime.now()
        
        let timestampsPtr = timestampsBuffer.contents().bindMemory(to: UInt64.self, capacity: 4)
        let timestamps = Array(UnsafeBufferPointer(start: timestampsPtr, count: 4))
        
        let totalTime = Double(endTime.uptimeNanoseconds - startTime.uptimeNanoseconds) / 1_000_000_000.0
        let gpuTime = Double(max(timestamps[1], timestamps[3]) - min(timestamps[0], timestamps[2])) / Double(NSEC_PER_SEC)
        let averageLatency = gpuTime / Double(iterations) * 1e6  // Convert to microseconds
        
        return averageLatency
    }
}
