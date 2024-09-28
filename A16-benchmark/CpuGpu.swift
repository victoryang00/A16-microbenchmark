//
//  cpu_gpu.swift
//  A16-benchmark
//
//  Created by yiwei yang on 9/25/22.
//

import Foundation
import MetalKit
/// https://developer.apple.com/documentation/metal/resource_synchronization/synchronizing_cpu_and_gpu_work

// | `Shared`     | *default* on `macOS` buffers, `iOS/tvOS` resources; not available on `macOS` textures. |
// | ------------ | ------------------------------------------------------------ |
// | `Private`    | mostly use when data is only accessed by `GPU`.              |
// | `Memoryless` | only for `iOS/tvOS` on-chip temporary render targets (textures). |
// | `Managed`    | *default* mode for `macOS` textures; not available on `iOS/tvOS` resources. |

/// https://linuxtut.com/en/94ddf03202517c64ca07/


func CpuGpu()->String{
    guard let device = MTLCreateSystemDefaultDevice() else {fatalError()}
    var res: String = "CPU GPU: "
    
    let count = 2000
    let length = count * MemoryLayout< Float >.stride
    var myBuffer: MTLBuffer!
    //  1. makeBuffer(length:)
    //
    var now = CACurrentMediaTime()
    myBuffer = device.makeBuffer(length: length, options: [])
    var now1 = CACurrentMediaTime()
    print(myBuffer.contents())
    res += "private" + (now1 -now)
    //  2. makeBuffer(bytes:)
    //
    var myVector = [Float](repeating: 0, count: count)
    now = CACurrentMediaTime()
    myBuffer = device.makeBuffer(bytes: myVector, length: length, options: [])
    withUnsafePointer(to: &myVector) { print($0) }
    print(myBuffer.contents())
    res += "shared" + now - now1
    //  3. makeBuffer(bytesNoCopy:)
    //
    var memory: UnsafeMutableRawPointer? = nil
    
    let alignment = 0x1000
    let allocationSize = (length + alignment - 1) & (~(alignment - 1))
    posix_memalign(&memory, alignment, allocationSize)
    now = TimeSpecification(clock: .system)
    myBuffer = device.makeBuffer(bytesNoCopy: memory!,
                                 length: allocationSize,
                                 options: [],
                                 deallocator: { (pointer: UnsafeMutableRawPointer, _: Int) in
        free(pointer)
    })
    res += "managed" + Date(timeIntervalSince1970: now).formatted()
    print(memory!)
    print (myBuffer!.contents())
    return res
}
