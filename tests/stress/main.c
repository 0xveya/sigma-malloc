#include "stress.h"
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool parse_u64(const char *text, uint64_t *out) {
  char *end = NULL;

  errno = 0;
  unsigned long long value = strtoull(text, &end, 10);

  if (errno != 0 || end == text || *end != '\0')
    return false;

  *out = (uint64_t)value;
  return true;
}

static bool parse_size(const char *text, uint64_t *out) {
  char *end = NULL;

  errno = 0;
  unsigned long long value = strtoull(text, &end, 10);

  if (errno != 0 || end == text)
    return false;

  uint64_t multiplier = 1;

  if (*end != '\0') {
    if (end[1] != '\0')
      return false;

    switch (*end) {
    case 'K':
    case 'k':
      multiplier = 1024ULL;
      break;
    case 'M':
    case 'm':
      multiplier = 1024ULL * 1024;
      break;
    case 'G':
    case 'g':
      multiplier = 1024ULL * 1024 * 1024;
      break;
    default:
      return false;
    }
  }

  if (value > UINT64_MAX / multiplier)
    return false;

  *out = (uint64_t)value * multiplier;
  return true;
}

static void usage(FILE *stream, const char *program) {
  fprintf(stream,
          "Usage: %s [options]\n"
          "\n"
          "  --allocator system|custom\n"
          "  --threads N\n"
          "  --target SIZE              Examples: 512M, 8G\n"
          "  --slots N\n"
          "  --cycles N                 0 means indefinitely\n"
          "  --seed N\n"
          "  --min-size SIZE\n"
          "  --max-size SIZE\n"
          "  --verify none|sample|full\n"
          "  --fragment-percent N\n"
          "  --stats-interval MS\n"
          "  --abort-on-error\n"
          "  -h, --help\n",
          program);
}

static bool parse_options(int argc, char **argv, Options *options) {
  enum {
    OPT_ALLOCATOR = 1000,
    OPT_THREADS,
    OPT_TARGET,
    OPT_SLOTS,
    OPT_CYCLES,
    OPT_SEED,
    OPT_MIN_SIZE,
    OPT_MAX_SIZE,
    OPT_VERIFY,
    OPT_FRAGMENT_PERCENT,
    OPT_STATS_INTERVAL,
    OPT_ABORT_ON_ERROR,
  };

  static const struct option long_options[] = {
      {"allocator", required_argument, NULL, OPT_ALLOCATOR},
      {"threads", required_argument, NULL, OPT_THREADS},
      {"target", required_argument, NULL, OPT_TARGET},
      {"slots", required_argument, NULL, OPT_SLOTS},
      {"cycles", required_argument, NULL, OPT_CYCLES},
      {"seed", required_argument, NULL, OPT_SEED},
      {"min-size", required_argument, NULL, OPT_MIN_SIZE},
      {"max-size", required_argument, NULL, OPT_MAX_SIZE},
      {"verify", required_argument, NULL, OPT_VERIFY},
      {"fragment-percent", required_argument, NULL, OPT_FRAGMENT_PERCENT},
      {"stats-interval", required_argument, NULL, OPT_STATS_INTERVAL},
      {"abort-on-error", no_argument, NULL, OPT_ABORT_ON_ERROR},
      {"help", no_argument, NULL, 'h'},
      {NULL, 0, NULL, 0},
  };

  for (;;) {
    int option = getopt_long(argc, argv, "h", long_options, NULL);

    if (option == -1)
      break;

    uint64_t value;

    switch (option) {
    case OPT_ALLOCATOR:
      if (strcmp(optarg, "system") == 0) {
        options->allocator = &system_allocator;
      } else if (strcmp(optarg, "custom") == 0) {
        options->allocator = &custom_allocator;
      } else {
        fprintf(stderr, "Unknown allocator: %s\n", optarg);
        return false;
      }
      break;

    case OPT_THREADS:
      if (!parse_u64(optarg, &value) || value == 0 || value > UINT_MAX) {
        fprintf(stderr, "Invalid thread count: %s\n", optarg);
        return false;
      }
      options->threads = (unsigned)value;
      break;

    case OPT_TARGET:
      if (!parse_size(optarg, &options->target_bytes)) {
        fprintf(stderr, "Invalid target size: %s\n", optarg);
        return false;
      }
      break;

    case OPT_SLOTS:
      if (!parse_u64(optarg, &options->slots) || options->slots == 0) {
        fprintf(stderr, "Invalid slot count: %s\n", optarg);
        return false;
      }
      break;

    case OPT_CYCLES:
      if (!parse_u64(optarg, &options->cycles)) {
        fprintf(stderr, "Invalid cycle count: %s\n", optarg);
        return false;
      }
      break;

    case OPT_SEED:
      if (!parse_u64(optarg, &options->seed)) {
        fprintf(stderr, "Invalid seed: %s\n", optarg);
        return false;
      }
      break;

    case OPT_MIN_SIZE:
      if (!parse_size(optarg, &options->min_size)) {
        fprintf(stderr, "Invalid minimum size: %s\n", optarg);
        return false;
      }
      break;

    case OPT_MAX_SIZE:
      if (!parse_size(optarg, &options->max_size)) {
        fprintf(stderr, "Invalid maximum size: %s\n", optarg);
        return false;
      }
      break;

    case OPT_VERIFY:
      if (strcmp(optarg, "none") == 0)
        options->verify = VERIFY_NONE;
      else if (strcmp(optarg, "sample") == 0)
        options->verify = VERIFY_SAMPLE;
      else if (strcmp(optarg, "full") == 0)
        options->verify = VERIFY_FULL;
      else {
        fprintf(stderr, "Invalid verification mode: %s\n", optarg);
        return false;
      }
      break;

    case OPT_FRAGMENT_PERCENT:
      if (!parse_u64(optarg, &value) || value > 100) {
        fprintf(stderr, "Invalid fragmentation percentage: %s\n", optarg);
        return false;
      }
      options->fragment_percent = (unsigned)value;
      break;

    case OPT_STATS_INTERVAL:
      if (!parse_u64(optarg, &value) || value > UINT_MAX) {
        fprintf(stderr, "Invalid statistics interval: %s\n", optarg);
        return false;
      }
      options->stats_interval_ms = (unsigned)value;
      break;

    case OPT_ABORT_ON_ERROR:
      options->abort_on_error = true;
      break;

    case 'h':
      usage(stdout, argv[0]);
      exit(EXIT_SUCCESS);

    default:
      return false;
    }
  }

  if (optind != argc) {
    fprintf(stderr, "Unexpected positional argument: %s\n", argv[optind]);
    return false;
  }

  if (options->min_size > options->max_size) {
    fprintf(stderr, "--min-size cannot exceed --max-size\n");
    return false;
  }

  if (options->target_bytes < options->min_size) {
    fprintf(stderr, "--target must be at least --min-size\n");
    return false;
  }

  return true;
}

int run_stress_test(const Options *options) {
  size_t size = (size_t)options->min_size;
  if ((uint64_t)size != options->min_size || size == 0) {
    fprintf(stderr, "Invalid test allocation size: %" PRIu64 "\n",
            options->min_size);
    return EXIT_FAILURE;
  }

  void *ptr = allocator_alloc(options->allocator, size);
  if (ptr == NULL) {
    fprintf(stderr, "%s allocation of %zu bytes failed\n",
            options->allocator->name, size);
    return EXIT_FAILURE;
  }

  memset(ptr, 0xa5, size < 64 ? size : 64);

  if (options->allocator == &custom_allocator && sigma_debug_enabled()) {
    if (sigma_debug_collect_leaks() == 0) {
      fprintf(stderr, "Sigma debug allocation was not tracked\n");
      options->allocator->deallocate(ptr);
      return EXIT_FAILURE;
    }
  }

  options->allocator->deallocate(ptr);

  if (options->allocator == &custom_allocator && sigma_debug_enabled() &&
      sigma_debug_collect_leaks() != 0) {
    fprintf(stderr, "Sigma debug allocation remained tracked after free\n");
    sigma_debug_reset_leaks();
    return EXIT_FAILURE;
  }

  printf("stress-test stub passed: allocator=%s size=%zu bytes\n",
         options->allocator->name, size);
  return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
  Options options = options_default();

  if (!parse_options(argc, argv, &options)) {
    usage(stderr, argv[0]);
    return EXIT_FAILURE;
  }

  return run_stress_test(&options);
}
