#import <XCTest/XCTest.h>
#import <objc/runtime.h>

#include "mime_parser_serialization_tests.h"

@interface MIMEParserSerializationTests : XCTestCase
- (void)runFixture:(NSString *)relativePath;
@end

static NSDictionary<NSString *, NSString *> *fixturePathsBySelector;

static void runSerializationFixture(id object, SEL selector)
{
  NSString *relativePath = fixturePathsBySelector[NSStringFromSelector(selector)];
  [(MIMEParserSerializationTests *) object runFixture:relativePath];
}

@implementation MIMEParserSerializationTests

+ (void)load
{
  NSString *root = [[NSBundle bundleForClass:self]
      pathForResource:@"mime-parser-serialization-data" ofType:nil];
  NSString *inputRoot = [root stringByAppendingPathComponent:@"input"];
  NSDirectoryEnumerator<NSString *> *enumerator =
      [[NSFileManager defaultManager] enumeratorAtPath:inputRoot];
  NSMutableDictionary<NSString *, NSString *> *paths =
      [NSMutableDictionary dictionary];
  NSMutableArray<NSString *> *relativePaths = [NSMutableArray array];
  NSString *relativePath;

  while ((relativePath = [enumerator nextObject]) != nil) {
    BOOL directory = NO;
    NSString *fullPath = [inputRoot stringByAppendingPathComponent:relativePath];
    if ([[NSFileManager defaultManager] fileExistsAtPath:fullPath
                                            isDirectory:&directory] && !directory)
      [relativePaths addObject:relativePath];
  }

  [relativePaths sortUsingSelector:@selector(compare:)];
  NSCharacterSet *invalid = [[NSCharacterSet alphanumericCharacterSet]
      invertedSet];
  for (relativePath in relativePaths) {
    NSArray<NSString *> *components = [relativePath
        componentsSeparatedByCharactersInSet:invalid];
    NSString *suffix = [components componentsJoinedByString:@"_"];
    NSString *selectorName = [@"testFixture_" stringByAppendingString:suffix];
    NSUInteger collision = 2;
    NSString *uniqueName = selectorName;
    while (paths[uniqueName] != nil)
      uniqueName = [selectorName stringByAppendingFormat:@"_%lu",
          (unsigned long) collision++];
    paths[uniqueName] = relativePath;
    class_addMethod(self, NSSelectorFromString(uniqueName),
        (IMP) runSerializationFixture, "v@:");
  }
  fixturePathsBySelector = [paths copy];
}

static void recordSerializationFailure(const char *file, unsigned line,
    const char *expression, const char *message, void *context)
{
  MIMEParserSerializationTests *test =
      (__bridge MIMEParserSerializationTests *) context;
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
      pathForResource:@"mime-parser-serialization-data" ofType:nil];
  XCTAssertNotNil(root,
      @"The MIME serialization fixtures are missing from the test bundle");
  if (root == nil || relativePath == nil)
    return;

  int result = mime_parser_serialization_test_run_case(
      relativePath.fileSystemRepresentation, root.fileSystemRepresentation,
      recordSerializationFailure, (__bridge void *) self);
  XCTAssertEqual(result, 0);
}

@end
