//
//  A16_benchmark_wrapper.m
//  A16-benchmark
//
//  Created by yiwei yang on 9/24/22.
//

#import <Foundation/Foundation.h>
#import "A16_benchmark_wrapper.h"
#import "rob.hpp"

@implementation ROBWrapper

- (NSString *) getROB {
    ROB rob;
    std::string ROBMessage = rob.getROB();
    return [NSString
            stringWithCString:ROBMessage.c_str()
            encoding:NSUTF8StringEncoding];
}

@end
