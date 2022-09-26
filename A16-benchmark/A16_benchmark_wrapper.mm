//
//  A16_benchmark_wrapper.m
//  A16-benchmark
//
//  Created by yiwei yang on 9/24/22.
//

#import <Foundation/Foundation.h>
#import "A16_benchmark_wrapper.h"
#import "rob.hpp"
#import "c2c.hpp"

@implementation ROBWrapper

- (NSString *) getROB {
    ROB rob;
    std::string ROBMessage = rob.getROB();
    return [NSString
            stringWithCString:ROBMessage.c_str()
            encoding:NSUTF8StringEncoding];
}

@end

@implementation C2CWrapper

- (NSString *) getC2C {
    C2C rob;
    std::string C2CMessage = rob.getC2C();
    return [NSString
            stringWithCString:C2CMessage.c_str()
            encoding:NSUTF8StringEncoding];
}

@end
