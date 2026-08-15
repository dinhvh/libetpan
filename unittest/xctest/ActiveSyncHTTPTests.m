#import <XCTest/XCTest.h>
#import <objc/runtime.h>

#include "activesync_tests.h"

@interface ActiveSyncHTTPTests : XCTestCase
- (void)runCase:(NSNumber *)index;
@end

static NSDictionary<NSString *, NSNumber *> *httpIndexesBySelector;

static void runHTTPCase(id object, SEL selector)
{
  NSNumber *index = httpIndexesBySelector[NSStringFromSelector(selector)];
  [(ActiveSyncHTTPTests *) object runCase:index];
}

@implementation ActiveSyncHTTPTests

+ (void)load
{
  NSMutableDictionary<NSString *, NSNumber *> *indexes =
      [NSMutableDictionary dictionary];
  for (size_t index = 0; index < activesync_http_test_count(); index++) {
    NSString *name = [NSString stringWithUTF8String:
        activesync_http_test_name(index)];
    if ([name hasPrefix:@"test_"])
      name = [name substringFromIndex:5];
    NSArray<NSString *> *words = [name componentsSeparatedByString:@"_"];
    NSMutableString *selectorName = [NSMutableString stringWithString:@"test"];
    for (NSString *word in words)
      [selectorName appendString:word.capitalizedString];
    indexes[selectorName] = @(index);
    class_addMethod(self, NSSelectorFromString(selectorName),
        (IMP) runHTTPCase, "v@:");
  }
  httpIndexesBySelector = [indexes copy];
}

static void recordHTTPFailure(const char *file, unsigned line,
    const char *expression, const char *message, void *context)
{
  ActiveSyncHTTPTests *test = (__bridge ActiveSyncHTTPTests *) context;
  NSString *description = [NSString stringWithFormat:@"%s: %s",
      expression, message];
  XCTSourceCodeLocation *location = [[XCTSourceCodeLocation alloc]
      initWithFilePath:[NSString stringWithUTF8String:file] lineNumber:line];
  XCTIssue *issue = [[XCTIssue alloc]
      initWithType:XCTIssueTypeAssertionFailure
      compactDescription:description
      detailedDescription:nil
      sourceCodeContext:[[XCTSourceCodeContext alloc] initWithLocation:location]
      associatedError:nil
      attachments:@[]];
  [test recordIssue:issue];
}

- (void)runCase:(NSNumber *)index
{
  XCTAssertNotNil(index);
  if (index == nil)
    return;
  int result = activesync_http_test_run_case(index.unsignedIntegerValue,
      recordHTTPFailure, (__bridge void *) self);
  XCTAssertEqual(result, 0);
}

@end
