// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: jdtang@google.com (Jonathan Tang)
//

#include <dirent.h>
#include <fstream>
#include <iostream>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <time.h>

#include "gumbo.h"

static const int kNumReps = 10;

int main(int argc, char** argv) {
  if (argc != 1) {
    std::cout << "Usage: benchmarks\n";
    exit(EXIT_FAILURE);
  }

  DIR* dir;
  struct dirent* file;

  if ((dir = opendir("benchmarks")) == NULL) {
    std::cout << "Couldn't find 'benchmarks' directory.  "
              << "Run from root of distribution.\n";
    exit(EXIT_FAILURE);
  }

  while ((file = readdir(dir)) != NULL) {
    std::string filename(file->d_name);
    if (filename.length() > 5 && filename.compare(filename.length() - 5, 5, ".html") == 0) {
      std::string full_filename = "benchmarks/" + filename;
      std::ifstream in(full_filename.c_str(), std::ios::in | std::ios::binary);
      if (!in) {
        std::cout << "File " << full_filename << " couldn't be read!\n";
        exit(EXIT_FAILURE);
      }

      std::string contents;
      in.seekg(0, std::ios::end);
      contents.resize(in.tellg());
      in.seekg(0, std::ios::beg);
      in.read(&contents[0], contents.size());
      in.close();

      clock_t start_time = clock();
      for (int i = 0; i < kNumReps; ++i) {
        GumboOutput* output = gumbo_parse(contents.c_str());
        gumbo_destroy_output(&kGumboDefaultOptions, output);
      }
      clock_t end_time = clock();
      std::cout << filename << ": "
          << (1000000 * (end_time - start_time) / (kNumReps * CLOCKS_PER_SEC))
          << " microseconds.\n";
    }
  }
  closedir(dir);
}
