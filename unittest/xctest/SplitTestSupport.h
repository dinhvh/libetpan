#import <Foundation/Foundation.h>

#include <limits.h>
#include <unistd.h>

static int RunTestFromRepositoryRoot(int (*test)(void))
{
  char previousDirectory[PATH_MAX];
  NSString *sourcePath = [NSString stringWithUTF8String:__FILE__];
  NSString *root = [[[sourcePath stringByDeletingLastPathComponent]
      stringByDeletingLastPathComponent] stringByDeletingLastPathComponent];

  if ((getcwd(previousDirectory, sizeof(previousDirectory)) == NULL) ||
      (chdir(root.fileSystemRepresentation) != 0))
    return 0;
  int result = test();
  if (chdir(previousDirectory) != 0)
    return 0;
  return result;
}
