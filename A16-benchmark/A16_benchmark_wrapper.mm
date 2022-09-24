//
//  A16_benchmark_wrapper.m
//  A16-benchmark
//
//  Created by yiwei yang on 9/24/22.
//

#import <Foundation/Foundation.h>
#import "A16_benchmark_wrapper.h"
#import "rob.hpp"

@implementation HelloWorldWrapper

- (NSString *) sayHello {
    HelloWorld helloWorld;
    std::string helloWorldMessage = helloWorld.sayHello();
    return [NSString
            stringWithCString:helloWorldMessage.c_str()
            encoding:NSUTF8StringEncoding];
}

@end
