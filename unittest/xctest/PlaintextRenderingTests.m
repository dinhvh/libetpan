#import <XCTest/XCTest.h>
#import <objc/runtime.h>

#include "plaintext_rendering_tests.h"

@interface PlaintextRenderingTests : XCTestCase
- (void)runFixture:(NSString *)relativePath;
@end

static NSDictionary<NSString *, NSString *> *plaintextPathsBySelector;

static void runPlaintextFixture(id object, SEL selector)
{
  NSString *relativePath =
      plaintextPathsBySelector[NSStringFromSelector(selector)];
  [(PlaintextRenderingTests *) object runFixture:relativePath];
}

@implementation PlaintextRenderingTests

+ (void)load
{
  NSString *root = [[NSBundle bundleForClass:self]
      pathForResource:@"plaintext-rendering-data" ofType:nil];
  NSString *inputRoot = [root stringByAppendingPathComponent:@"input"];
  NSArray<NSString *> *relativePaths = [[[NSFileManager defaultManager]
      contentsOfDirectoryAtPath:inputRoot error:nil]
      sortedArrayUsingSelector:@selector(compare:)];
  NSMutableDictionary<NSString *, NSString *> *paths =
      [NSMutableDictionary dictionary];
  NSCharacterSet *invalid = [[NSCharacterSet alphanumericCharacterSet]
      invertedSet];

  for (NSString *relativePath in relativePaths) {
    NSArray<NSString *> *components = [relativePath
        componentsSeparatedByCharactersInSet:invalid];
    NSString *selectorName = [@"testFixture_" stringByAppendingString:
        [components componentsJoinedByString:@"_"]];
    paths[selectorName] = relativePath;
    class_addMethod(self, NSSelectorFromString(selectorName),
        (IMP) runPlaintextFixture, "v@:");
  }
  plaintextPathsBySelector = [paths copy];
}

static void recordPlaintextFailure(const char *file, unsigned line,
    const char *expression, const char *message, void *context)
{
  PlaintextRenderingTests *test =
      (__bridge PlaintextRenderingTests *) context;
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

- (void)runFixture:(NSString *)relativePath
{
  XCTAssertNotNil(relativePath);
  NSString *root = [[NSBundle bundleForClass:self.class]
      pathForResource:@"plaintext-rendering-data" ofType:nil];
  XCTAssertNotNil(root,
      @"The plaintext rendering fixtures are missing from the test bundle");
  if (root == nil || relativePath == nil)
    return;

  int result = plaintext_rendering_test_run_case(
      relativePath.fileSystemRepresentation, root.fileSystemRepresentation,
      recordPlaintextFailure, (__bridge void *) self);
  XCTAssertEqual(result, 0);
}

@end
