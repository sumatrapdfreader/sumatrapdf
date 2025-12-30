#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#if !defined(_MSC_VER)
#include <glob.h>
#endif
#include <vector>

#include "deorummolae.h"
#include "durchschlag.h"
#include "sieve.h"

/* This isn't a definitive list of "--foo" arguments, only those that take an
 * additional "=#" integer parameter, like "--foo=20" or "--foo=32K".
 */
#define LONG_ARG_BLOCK_LEN "--block_len="
#define LONG_ARG_SLICE_LEN "--slice_len="
#define LONG_ARG_TARGET_DICT_LEN "--target_dict_len="
#define LONG_ARG_MIN_SLICE_POP "--min_slice_pop="
#define LONG_ARG_CHUNK_LEN "--chunk_len="
#define LONG_ARG_OVERLAP_LEN "--overlap_len="

#define METHOD_DM 0
#define METHOD_SIEVE 1
#define METHOD_DURCHSCHLAG 2
#define METHOD_DISTILL 3
#define METHOD_PURIFY 4

static size_t readInt(const char* str) {
  size_t result = 0;
  if (str[0] == 0 || str[0] == '0') {
    return 0;
  }
  for (size_t i = 0; i < 13; ++i) {
    if (str[i] == 0) {
      return result;
    }
    if (str[i] == 'k' || str[i] == 'K') {
      if ((str[i + 1] == 0) && ((result << 10) > result)) {
        return result << 10;
      }
      return 0;
    }
    if (str[i] == 'm' || str[i] == 'M') {
      if ((str[i + 1] == 0) && ((result << 20) > result)) {
        return result << 20;
      }
      return 0;
    }
    if (str[i] < '0' || str[i] > '9') {
      return 0;
    }
    size_t next = (10 * result) + (str[i] - '0');
    if (next <= result) {
      return 0;
    }
    result = next;
  }
  return 0;
}

static std::string readFile(const std::string& path) {
  std::ifstream file(path);
  std::string content(
      (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return content;
}

static void writeFile(const char* file, const std::string& content) {
  std::ofstream outfile(file, std::ofstream::binary);
  outfile.write(content.c_str(), static_cast<std::streamsize>(content.size()));
  outfile.close();
}

static void writeSamples(const std::vector<std::string>& paths,
    const std::vector<size_t>& sizes, const uint8_t* data) {
  size_t offset = 0;
  for (size_t i = 0; i < paths.size(); ++i) {
    const char* path = paths[i].c_str();
    size_t sampleSize = sizes[i];
    std::ofstream outfile(path, std::ofstream::binary);
    outfile.write(reinterpret_cast<const char*>(data + offset),
        static_cast<std::streamsize>(sampleSize));
    outfile.close();
    offset += sampleSize;
  }
}

/* Returns "base file name" or its tail, if it contains '/' or '\'. */
static const char* fileName(const char* path) {
  const char* separator_position = strrchr(path, '/');
  if (separator_position) path = separator_position + 1;
  separator_position = strrchr(path, '\\');
  if (separator_position) path = separator_position + 1;
  return path;
}

static void printHelp(const char* name) {
  fprintf(stderr, "Usage: %s [OPTION]... DICTIONARY [SAMPLE]...\n", name);
  fprintf(stderr,
      "Options:\n"
      "  --dm       use 'deorummolae' engine\n"
      "  --distill  rewrite samples; unique text parts are removed\n"
      "  --dsh      use 'durchschlag' engine (default)\n"
      "  --purify   rewrite samples; unique text parts are zeroed out\n"
      "  --sieve    use 'sieve' engine\n"
      "  -b#, --block_len=#\n"
      "             set block length for 'durchschlag'; default: 1024\n"
      "  -s#, --slice_len=#\n"
      "             set slice length for 'distill', 'durchschlag', 'purify'\n"
      "             and 'sieve'; default: 16\n"
      "  -t#, --target_dict_len=#\n"
      "             set target dictionary length (limit); default: 16K\n"
      "  -u#, --min_slice_pop=#\n"
      "             set minimum slice population (for rewrites); default: 2\n"
      "  -c#, --chunk_len=#\n"
      "             if positive, samples are cut into chunks of this length;\n"
      "             default: 0; cannot mix with 'rewrite samples'\n"
      "  -o#, --overlap_len=#\n"
      "             set chunk overlap length; default 0\n"
      "# is a decimal number with optional k/K/m/M suffix.\n"
      "WARNING: 'distill' and 'purify' will overwrite original samples!\n"
      "         Completely unique samples might become empty files.\n\n");
}

int main(int argc, char const* argv[]) {
  int dictionaryArg = -1;
  int method = METHOD_DURCHSCHLAG;
  size_t sliceLen = 16;
  size_t targetSize = 16 << 10;
  size_t blockSize = 1024;
  size_t minimumPopulation = 2;
  size_t chunkLen = 0;
  size_t overlapLen = 0;

  std::vector<uint8_t> data;
  std::vector<size_t> sizes;
  std::vector<std::string> paths;
  size_t total = 0;
  for (int i = 1; i < argc; ++i) {
    if (argv[i] == nullptr) {
      continue;
    }

    if (argv[i][0] == '-') {
      char arg1 = argv[i][1];
      const char* arg2 = arg1 ? &argv[i][2] : nullptr;
      if (arg1 == '-') {
        if (dictionaryArg != -1) {
          fprintf(stderr,
              "Method should be specified before dictionary / sample '%s'\n",
              argv[i]);
          exit(1);
        }

        /* Look for "--long_arg" via exact match. */
        if (std::strcmp(argv[i], "--sieve") == 0) {
          method = METHOD_SIEVE;
          continue;
        }
        if (std::strcmp(argv[i], "--dm") == 0) {
          method = METHOD_DM;
          continue;
        }
        if (std::strcmp(argv[i], "--dsh") == 0) {
          method = METHOD_DURCHSCHLAG;
          continue;
        }
        if (std::strcmp(argv[i], "--distill") == 0) {
          method = METHOD_DISTILL;
          continue;
        }
        if (std::strcmp(argv[i], "--purify") == 0) {
          method = METHOD_PURIFY;
          continue;
        }

        /* Look for "--long_arg=#" via prefix match. */
        if (std::strncmp(argv[i], LONG_ARG_BLOCK_LEN,
              std::strlen(LONG_ARG_BLOCK_LEN)) == 0) {
          arg1 = 'b';
          arg2 = &argv[i][std::strlen(LONG_ARG_BLOCK_LEN)];
        } else if (std::strncmp(argv[i], LONG_ARG_SLICE_LEN,
              std::strlen(LONG_ARG_SLICE_LEN)) == 0) {
          arg1 = 's';
          arg2 = &argv[i][std::strlen(LONG_ARG_SLICE_LEN)];
        } else if (std::strncmp(argv[i], LONG_ARG_TARGET_DICT_LEN,
              std::strlen(LONG_ARG_TARGET_DICT_LEN)) == 0) {
          arg1 = 't';
          arg2 = &argv[i][std::strlen(LONG_ARG_TARGET_DICT_LEN)];
        } else if (std::strncmp(argv[i], LONG_ARG_MIN_SLICE_POP,
              std::strlen(LONG_ARG_MIN_SLICE_POP)) == 0) {
          arg1 = 'u';
          arg2 = &argv[i][std::strlen(LONG_ARG_MIN_SLICE_POP)];
        } else if (std::strncmp(argv[i], LONG_ARG_CHUNK_LEN,
              std::strlen(LONG_ARG_CHUNK_LEN)) == 0) {
          arg1 = 'c';
          arg2 = &argv[i][std::strlen(LONG_ARG_CHUNK_LEN)];
        } else if (std::strncmp(argv[i], LONG_ARG_OVERLAP_LEN,
              std::strlen(LONG_ARG_OVERLAP_LEN)) == 0) {
          arg1 = 'o';
          arg2 = &argv[i][std::strlen(LONG_ARG_OVERLAP_LEN)];
        } else {
          printHelp(fileName(argv[0]));
          fprintf(stderr, "Invalid option '%s'\n", argv[i]);
          exit(1);
        }
      }

      /* Look for "-f" short args or "--foo=#" long args. */
      if (arg1 == 'b') {
        blockSize = readInt(arg2);
        if (blockSize < 16 || blockSize > 65536) {
          printHelp(fileName(argv[0]));
          fprintf(stderr, "Invalid option '%s'\n", argv[i]);
          exit(1);
        }
      } else if (arg1 == 's') {
        sliceLen = readInt(arg2);
        // TODO(eustas): investigate why sliceLen == 4..5 greatly slows down
        //               durschlag engine, but only from command line;
        //               durschlag_runner seems to work fine with those.
        if (sliceLen < 4 || sliceLen > 256) {
          printHelp(fileName(argv[0]));
          fprintf(stderr, "Invalid option '%s'\n", argv[i]);
          exit(1);
        }
      } else if (arg1 == 't') {
        targetSize = readInt(arg2);
        if (targetSize < 256 || targetSize > (1 << 25)) {
          printHelp(fileName(argv[0]));
          fprintf(stderr, "Invalid option '%s'\n", argv[i]);
          exit(1);
        }
      } else if (arg1 == 'u') {
        minimumPopulation = readInt(arg2);
        if (minimumPopulation < 256 || minimumPopulation > 65536) {
          printHelp(fileName(argv[0]));
          fprintf(stderr, "Invalid option '%s'\n", argv[i]);
          exit(1);
        }
      } else if (arg1 == 'c') {
        chunkLen = readInt(arg2);
        if (chunkLen < 0 || chunkLen > INT_MAX) {
          printHelp(fileName(argv[0]));
          fprintf(stderr, "Invalid option '%s'\n", argv[i]);
          exit(1);
        }
      } else if (arg1 == 'o') {
        overlapLen = readInt(arg2);
        if (overlapLen < 0 || overlapLen > INT_MAX) {
          printHelp(fileName(argv[0]));
          fprintf(stderr, "Invalid option '%s'\n", argv[i]);
          exit(1);
        }
      } else {
        printHelp(fileName(argv[0]));
        fprintf(stderr, "Unrecognized option '%s'\n", argv[i]);
        exit(1);
      }
      continue;
    }

    if (dictionaryArg == -1) {
      if (method != METHOD_DISTILL && method != METHOD_PURIFY) {
        dictionaryArg = i;
        continue;
      }
    }

    bool ok = true;
#if defined(_MSC_VER)
        const char* resolved_path = argv[i];
#else
    glob_t resolved_paths;
    memset(&resolved_paths, 0, sizeof(resolved_paths));
    if (glob(argv[i], GLOB_TILDE, NULL, &resolved_paths) == 0) {
      for(size_t j = 0; j < resolved_paths.gl_pathc; ++j) {
        const char* resolved_path = resolved_paths.gl_pathv[j];
#endif
        std::string content = readFile(resolved_path);
        if (chunkLen == 0) {
          paths.emplace_back(resolved_path);
          data.insert(data.end(), content.begin(), content.end());
          total += content.size();
          sizes.push_back(content.size());
          continue;
        } else if (chunkLen <= overlapLen) {
          printHelp(fileName(argv[0]));
          fprintf(stderr, "Invalid chunkLen - overlapLen combination\n");
          exit(1);
        }
        for (size_t chunkStart = 0;
            chunkStart < content.size();
            chunkStart += chunkLen - overlapLen) {
          std::string chunk = content.substr(chunkStart, chunkLen);
          data.insert(data.end(), chunk.begin(), chunk.end());
          total += chunk.size();
          sizes.push_back(chunk.size());
        }
#if !defined(_MSC_VER)
      }
    } else {
      ok = false;
    }
    globfree(&resolved_paths);
#endif
    if (!ok) exit(1);
  }

  fprintf(stderr, "Number of chunks: %zu; total size: %zu\n", sizes.size(),
          total);

  bool wantDictionary = (dictionaryArg == -1);
  if (method == METHOD_DISTILL || method == METHOD_PURIFY) {
    wantDictionary = false;
    if (chunkLen != 0) {
      printHelp(fileName(argv[0]));
      fprintf(stderr, "Cannot mix 'rewrite samples' with positive chunk_len\n");
      exit(1);
    }
  }
  if (wantDictionary || total == 0) {
    printHelp(fileName(argv[0]));
    fprintf(stderr, "Not enough arguments\n");
    exit(1);
  }

  if (method == METHOD_SIEVE) {
    writeFile(argv[dictionaryArg], sieve_generate(
        targetSize, sliceLen, sizes, data.data()));
  } else if (method == METHOD_DM) {
    writeFile(argv[dictionaryArg], DM_generate(
        targetSize, sizes, data.data()));
  } else if (method == METHOD_DURCHSCHLAG) {
    writeFile(argv[dictionaryArg], durchschlag_generate(
        targetSize, sliceLen, blockSize, sizes, data.data()));
  } else if (method == METHOD_DISTILL) {
    durchschlag_distill(sliceLen, minimumPopulation, &sizes, data.data());
    writeSamples(paths, sizes, data.data());
  } else if (method == METHOD_PURIFY) {
    durchschlag_purify(sliceLen, minimumPopulation, sizes, data.data());
    writeSamples(paths, sizes, data.data());
  } else {
    printHelp(fileName(argv[0]));
    fprintf(stderr, "Unknown generator\n");
    exit(1);
  }
  return 0;
}
