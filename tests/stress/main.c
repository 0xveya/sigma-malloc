#define _POSIX_C_SOURCE 200809L
#include "stress.h"
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
  fprintf(
      stream,
      "Usage: %s [options]\n"
      "\n"
      "Size values accept bytes or a K, M, or G suffix (powers of 1024).\n"
      "\n"
      "Options:\n"
      "  --allocator system|custom\n"
      "      Allocator under test. 'system' uses malloc/free; 'custom' uses\n"
      "      sigma_malloc. Default: custom. Example: --allocator system\n"
      "\n"
      "  --threads N\n"
      "      Number of concurrent worker threads. Each worker gets its own\n"
      "      target, slots, and deterministic PRNG. Default: 1.\n"
      "      Example: --threads 8\n"
      "\n"
      "  --target SIZE\n"
      "      Requested live payload per worker. Allocator metadata and size\n"
      "      rounding can use more RAM. Default: 16M. Example: --target 2G\n"
      "\n"
      "  --slots N\n"
      "      Maximum simultaneously tracked allocations per worker. It must\n"
      "      be large enough to reach the target. Default: 4096.\n"
      "      Example: --slots 100000\n"
      "\n"
      "  --cycles N\n"
      "      Number of fill/fragment/refill/verify/drain cycles. Zero runs\n"
      "      until stopped or an error occurs. Default: 2.\n"
      "      Examples: --cycles 50, --cycles 0\n"
      "\n"
      "  --seed N\n"
      "      Unsigned deterministic workload seed. Reusing it reproduces each\n"
      "      worker's choices. Default: 12345. Example: --seed 987654\n"
      "\n"
      "  --min-size SIZE\n"
      "      Smallest generated allocation. Must not exceed max-size.\n"
      "      Default: 1 byte. Example: --min-size 16\n"
      "\n"
      "  --max-size SIZE\n"
      "      Largest generated allocation. Default: 64K.\n"
      "      Example: --max-size 32M\n"
      "\n"
      "  --verify none|sample|full\n"
      "      Data checking mode: none skips checks, sample checks five "
      "offsets,\n"
      "      and full checks every byte. Default: full.\n"
      "      Example: --verify sample\n"
      "\n"
      "  --output human|json\n"
      "      Human-readable text or newline-delimited JSON events. JSON "
      "records\n"
      "      include thread, phase, timing, memory, and result data.\n"
      "      Default: human. Example: --output json\n"
      "\n"
      "  --fragment-percent N\n"
      "      Percentage of occupied slots freed before refill, from 0 to 100.\n"
      "      Default: 50. Example: --fragment-percent 75\n"
      "\n"
      "  --stats-interval MS\n"
      "      Reserved statistics interval setting. Periodic reporting is not\n"
      "      implemented yet. Default: 1000. Example: --stats-interval 250\n"
      "\n"
      "  --abort-on-error\n"
      "      Abort immediately after detecting corruption. Enabled by default\n"
      "      in the current development configuration.\n"
      "\n"
      "  -h, --help\n"
      "      Print this reference and exit. Example: --help\n"
      "\n"
      "Workload examples:\n"
      "  Short correctness run:\n"
      "    %s --allocator custom --target 64M --max-size 1M "
      "--slots 8192 --cycles 2 --verify full --seed 12345\n"
      "\n"
      "  Thread-safety run:\n"
      "    %s --allocator custom --threads 4 --target 256M "
      "--max-size 1M --slots 8192 --cycles 20 --verify full "
      "--seed 12345\n"
      "\n"
      "  Long memory-heavy run (adjust 32G below available RAM):\n"
      "    %s --allocator system --target 32G --max-size 32M "
      "--slots 262144 --cycles 0 --verify sample --seed 12345 "
      "--output json\n"
      "\n"
      "The target counts requested payload bytes. Ensure slots * max-size "
      ">= target.\n",
      program, program, program, program);
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
    OPT_OUTPUT,
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
      {"output", required_argument, NULL, OPT_OUTPUT},
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

    case OPT_OUTPUT:
      if (strcmp(optarg, "human") == 0)
        options->output = OUTPUT_HUMAN;
      else if (strcmp(optarg, "json") == 0)
        options->output = OUTPUT_JSON;
      else {
        fprintf(stderr, "Invalid output format: %s\n", optarg);
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

static uint64_t rng_next(uint64_t *state) {
  uint64_t x = *state;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  *state = x;
  return x;
}

static uint64_t make_thread_seed(uint64_t seed, unsigned thread_index) {
  uint64_t result = seed ^ (0x9E3779B97F4A7C15ULL * (thread_index + 1));

  if (result == 0)
    result = 0xA5A5A5A5A5A5A5A5ULL;

  return result;
}

static uint64_t rng_bounded(uint64_t *state, uint64_t limit) {
  if (limit == 0)
    return 0;

  return rng_next(state) % limit;
}

static size_t choose_size(worker_state *worker) {
  static const size_t boundaries[] = {
      8,
      16,
      32,
      64,
      128,
      256,
      512,
      1024,
      4096,
      16384,
      65536,
      1024 * 1024,
      32 * 1024 * 1024,
  };

  size_t index = (size_t)rng_bounded(&worker->rng_state,
                                     sizeof boundaries / sizeof boundaries[0]);

  size_t upper = boundaries[index];

  if (upper > worker->options->max_size)
    upper = worker->options->max_size;

  if (upper < worker->options->min_size)
    upper = worker->options->min_size;

  size_t lower = worker->options->min_size;

  if (index > 0 && boundaries[index - 1] > lower)
    lower = boundaries[index - 1];

  if (lower > upper)
    lower = upper;

  if (upper <= lower)
    return lower;

  return lower + (size_t)rng_bounded(&worker->rng_state, upper - lower + 1);
}

static unsigned char expected_byte(const allocation_slot *slot, size_t offset) {
  uint64_t value = slot->pattern_seed ^ slot->allocation_id ^
                   ((uint64_t)offset * 0x9E3779B97F4A7C15ULL);

  value ^= value >> 33;
  value *= 0xFF51AFD7ED558CCDULL;
  value ^= value >> 33;

  return (unsigned char)value;
}

static void fill_allocation(allocation_slot *slot) {
  unsigned char *bytes = slot->ptr;

  for (size_t i = 0; i < slot->size; ++i)
    bytes[i] = expected_byte(slot, i);
}

static bool verify_allocation(const allocation_slot *slot,
                              unsigned thread_index, uint64_t seed) {
  const unsigned char *bytes = slot->ptr;

  for (size_t i = 0; i < slot->size; ++i) {
    unsigned char expected = expected_byte(slot, i);

    if (bytes[i] != expected) {
      fprintf(stderr,
              "CORRUPTION: thread=%u id=%llu ptr=%p "
              "size=%zu offset=%zu expected=%02x actual=%02x seed=%llu\n",
              thread_index, (unsigned long long)slot->allocation_id, slot->ptr,
              slot->size, i, expected, bytes[i], (unsigned long long)seed);

      return false;
    }
  }

  return true;
}

static bool verify_sample(const allocation_slot *slot, unsigned thread_index,
                          uint64_t seed) {
  if (slot->size == 0)
    return true;

  size_t offsets[] = {
      0, slot->size / 4, slot->size / 2, (slot->size * 3) / 4, slot->size - 1,
  };

  const unsigned char *bytes = slot->ptr;

  for (size_t i = 0; i < sizeof offsets / sizeof offsets[0]; ++i) {
    size_t offset = offsets[i];
    unsigned char expected = expected_byte(slot, offset);

    if (bytes[offset] != expected) {
      fprintf(stderr,
              "CORRUPTION: thread=%u id=%llu ptr=%p "
              "size=%zu offset=%zu expected=%02x actual=%02x seed=%llu\n",
              thread_index, (unsigned long long)slot->allocation_id, slot->ptr,
              slot->size, offset, expected, bytes[offset],
              (unsigned long long)seed);

      return false;
    }
  }

  return true;
}

static bool verify_slot(const worker_state *worker,
                        const allocation_slot *slot) {
  switch (worker->options->verify) {
  case VERIFY_NONE:
    return true;

  case VERIFY_SAMPLE:
    return verify_sample(slot, worker->thread_index, worker->options->seed);

  case VERIFY_FULL:
    return verify_allocation(slot, worker->thread_index, worker->options->seed);
  }

  return false;
}

static bool allocate_slot(worker_state *worker, allocation_slot *slot) {
  if (slot->ptr != NULL)
    return false;

  size_t size = choose_size(worker);

  if (size > worker->options->target_bytes - worker->live_bytes)
    size = worker->options->target_bytes - worker->live_bytes;

  if (size < worker->options->min_size)
    return false;

  void *ptr = allocator_alloc(worker->options->allocator, size);

  if (ptr == NULL) {
    ++worker->allocation_failures;
    return false;
  }

  slot->ptr = ptr;
  slot->size = size;
  slot->allocation_id = worker->next_allocation_id++;
  slot->pattern_seed = rng_next(&worker->rng_state);

  fill_allocation(slot);

  worker->live_bytes += size;
  if (worker->live_bytes > worker->peak_live_bytes)
    worker->peak_live_bytes = worker->live_bytes;
  ++worker->live_allocations;
  ++worker->allocation_count;

  return true;
}

static bool free_slot(worker_state *worker, allocation_slot *slot) {
  if (slot->ptr == NULL)
    return true;

  if (!verify_slot(worker, slot)) {
    if (worker->options->abort_on_error)
      abort();

    return false;
  }

  worker->options->allocator->deallocate(slot->ptr);
  worker->live_bytes -= slot->size;
  --worker->live_allocations;
  ++worker->free_count;
  *slot = (allocation_slot){0};
  return true;
}

static void fill_phase(worker_state *worker) {
  uint64_t failed_attempts = 0;

  while (worker->live_bytes < worker->options->target_bytes) {
    size_t index = (size_t)rng_bounded(&worker->rng_state, worker->slot_count);
    allocation_slot *slot = &worker->slots[index];

    if (slot->ptr != NULL) {
      ++failed_attempts;
      if (failed_attempts > (uint64_t)worker->slot_count * 4)
        break;
      continue;
    }

    failed_attempts = 0;
    if (!allocate_slot(worker, slot))
      break;
  }
}

static bool fragment_phase(worker_state *worker) {
  size_t occupied = 0;

  for (size_t i = 0; i < worker->slot_count; ++i) {
    if (worker->slots[i].ptr != NULL)
      ++occupied;
  }

  size_t to_free = occupied * worker->options->fragment_percent / 100;

  while (to_free > 0) {
    size_t index = (size_t)rng_bounded(&worker->rng_state, worker->slot_count);
    allocation_slot *slot = &worker->slots[index];

    if (slot->ptr == NULL)
      continue;
    if (!free_slot(worker, slot))
      return false;
    --to_free;
  }

  return true;
}

static bool verify_all_slots(worker_state *worker) {
  for (size_t i = 0; i < worker->slot_count; ++i) {
    allocation_slot *slot = &worker->slots[i];

    if (slot->ptr == NULL)
      continue;
    if (!verify_slot(worker, slot))
      return false;
    ++worker->verification_count;
  }

  return true;
}

static bool drain_phase(worker_state *worker) {
  bool success = true;

  for (size_t i = 0; i < worker->slot_count; ++i) {
    if (!free_slot(worker, &worker->slots[i]))
      success = false;
  }

  if (worker->live_bytes != 0) {
    fprintf(stderr, "Internal accounting error: live_bytes=%llu seed=%llu\n",
            (unsigned long long)worker->live_bytes,
            (unsigned long long)worker->options->seed);
    success = false;
  }

  return success;
}

static const char *verify_mode_name(VerifyMode verify) {
  switch (verify) {
  case VERIFY_NONE:
    return "none";
  case VERIFY_SAMPLE:
    return "sample";
  case VERIFY_FULL:
    return "full";
  }

  return "unknown";
}

typedef struct {
  uint64_t started_ns;
  uint64_t live_bytes;
  uint64_t operation_count;
} phase_snapshot;

static uint64_t monotonic_time_ns(void) {
  struct timespec time;

  if (clock_gettime(CLOCK_MONOTONIC, &time) != 0)
    return 0;

  return (uint64_t)time.tv_sec * 1000000000ULL + (uint64_t)time.tv_nsec;
}

static phase_snapshot emit_phase_begin(const worker_state *worker,
                                       uint64_t cycle, const char *phase) {
  if (worker->options->output == OUTPUT_HUMAN)
    printf("thread %u cycle %llu: %s\n", worker->thread_index,
           (unsigned long long)cycle, phase);

  return (phase_snapshot){
      .started_ns = monotonic_time_ns(),
      .live_bytes = worker->live_bytes,
      .operation_count = worker->allocation_count + worker->free_count +
                         worker->verification_count,
  };
}

static void emit_phase_end(const worker_state *worker, uint64_t cycle,
                           const char *phase, bool success,
                           phase_snapshot before) {
  uint64_t finished_ns = monotonic_time_ns();
  uint64_t duration_ns =
      finished_ns >= before.started_ns ? finished_ns - before.started_ns : 0;
  uint64_t bytes_changed = worker->live_bytes >= before.live_bytes
                               ? worker->live_bytes - before.live_bytes
                               : before.live_bytes - worker->live_bytes;
  uint64_t operation_count = worker->allocation_count + worker->free_count +
                             worker->verification_count;
  uint64_t operations = operation_count - before.operation_count;

  if (worker->options->output == OUTPUT_JSON) {
    printf("{\"event\":\"phase\",\"thread\":%u,\"cycle\":%llu,"
           "\"phase\":\"%s\","
           "\"success\":%s,\"duration_ns\":%llu,\"bytes_changed\":%llu,"
           "\"operations\":%llu,\"live_bytes\":%llu,"
           "\"live_allocations\":%llu}\n",
           worker->thread_index, (unsigned long long)cycle, phase,
           success ? "true" : "false", (unsigned long long)duration_ns,
           (unsigned long long)bytes_changed, (unsigned long long)operations,
           (unsigned long long)worker->live_bytes,
           (unsigned long long)worker->live_allocations);
    fflush(stdout);
  } else {
    printf("  thread=%u duration=%llu ns bytes-changed=%llu operations=%llu "
           "live=%llu allocations=%llu\n",
           worker->thread_index, (unsigned long long)duration_ns,
           (unsigned long long)bytes_changed, (unsigned long long)operations,
           (unsigned long long)worker->live_bytes,
           (unsigned long long)worker->live_allocations);
  }
}

static bool run_cycle(worker_state *worker, uint64_t cycle) {
  phase_snapshot phase = emit_phase_begin(worker, cycle, "fill");
  fill_phase(worker);
  emit_phase_end(worker, cycle, "fill", true, phase);

  phase = emit_phase_begin(worker, cycle, "fragment");
  bool success = fragment_phase(worker);
  emit_phase_end(worker, cycle, "fragment", success, phase);
  if (!success)
    return false;

  phase = emit_phase_begin(worker, cycle, "refill");
  fill_phase(worker);
  emit_phase_end(worker, cycle, "refill", true, phase);

  phase = emit_phase_begin(worker, cycle, "verify");
  success = verify_all_slots(worker);
  emit_phase_end(worker, cycle, "verify", success, phase);
  if (!success)
    return false;

  phase = emit_phase_begin(worker, cycle, "drain");
  success = drain_phase(worker);
  emit_phase_end(worker, cycle, "drain", success, phase);
  return success;
}

static bool run_worker(const Options *options, unsigned thread_index) {
  worker_state worker = {
      .options = options,
      .thread_index = thread_index,
      .rng_state = make_thread_seed(options->seed, thread_index),
      .next_allocation_id = 1,
      .slot_count = (size_t)options->slots,
  };

  worker.slots = calloc(worker.slot_count, sizeof *worker.slots);
  if (worker.slots == NULL) {
    fprintf(stderr, "Could not allocate slot table\n");
    return false;
  }

  uint64_t worker_started_ns = monotonic_time_ns();

  if (options->output == OUTPUT_JSON) {
    printf(
        "{\"event\":\"start\",\"allocator\":\"%s\",\"thread\":%u,"
        "\"seed\":%llu,\"worker_seed\":%llu,\"target_bytes\":%llu,"
        "\"min_size\":%llu,\"max_size\":%llu,\"slots\":%llu,"
        "\"cycles\":%llu,\"fragment_percent\":%u,\"verify\":\"%s\"}\n",
        options->allocator->name, thread_index,
        (unsigned long long)options->seed, (unsigned long long)worker.rng_state,
        (unsigned long long)options->target_bytes,
        (unsigned long long)options->min_size,
        (unsigned long long)options->max_size,
        (unsigned long long)options->slots, (unsigned long long)options->cycles,
        options->fragment_percent, verify_mode_name(options->verify));
    fflush(stdout);
  } else {
    printf("allocator=%s thread=%u seed=%llu worker-seed=%llu\n",
           options->allocator->name, thread_index,
           (unsigned long long)options->seed,
           (unsigned long long)worker.rng_state);
  }

  bool success = true;

  if (options->cycles == 0) {
    for (uint64_t cycle = 1;; ++cycle) {
      if (!run_cycle(&worker, cycle)) {
        success = false;
        break;
      }
    }
  } else {
    for (uint64_t cycle = 1; cycle <= options->cycles; ++cycle) {
      if (!run_cycle(&worker, cycle)) {
        success = false;
        break;
      }
    }
  }

  if (!drain_phase(&worker))
    success = false;

  uint64_t worker_finished_ns = monotonic_time_ns();
  uint64_t total_duration_ns = worker_finished_ns >= worker_started_ns
                                   ? worker_finished_ns - worker_started_ns
                                   : 0;

  if (options->output == OUTPUT_JSON) {
    printf("{\"event\":\"summary\",\"thread\":%u,\"success\":%s,"
           "\"allocations\":%llu,"
           "\"frees\":%llu,\"failures\":%llu,\"verifications\":%llu,"
           "\"live_bytes\":%llu,\"peak_live_bytes\":%llu,"
           "\"total_duration_ns\":%llu,\"seed\":%llu}\n",
           thread_index, success ? "true" : "false",
           (unsigned long long)worker.allocation_count,
           (unsigned long long)worker.free_count,
           (unsigned long long)worker.allocation_failures,
           (unsigned long long)worker.verification_count,
           (unsigned long long)worker.live_bytes,
           (unsigned long long)worker.peak_live_bytes,
           (unsigned long long)total_duration_ns,
           (unsigned long long)options->seed);
    fflush(stdout);
  } else {
    printf("thread=%u allocations=%llu frees=%llu failures=%llu "
           "verifications=%llu live=%llu peak-live=%llu duration=%llu ns\n",
           thread_index, (unsigned long long)worker.allocation_count,
           (unsigned long long)worker.free_count,
           (unsigned long long)worker.allocation_failures,
           (unsigned long long)worker.verification_count,
           (unsigned long long)worker.live_bytes,
           (unsigned long long)worker.peak_live_bytes,
           (unsigned long long)total_duration_ns);
  }

  free(worker.slots);
  return success;
}

typedef struct {
  pthread_mutex_t mutex;
  pthread_cond_t condition;
  bool start;
} worker_start_gate;

typedef struct {
  const Options *options;
  unsigned thread_index;
  worker_start_gate *gate;
  bool success;
} worker_thread_context;

static void *run_worker_thread(void *argument) {
  worker_thread_context *context = argument;

  pthread_mutex_lock(&context->gate->mutex);
  while (!context->gate->start)
    pthread_cond_wait(&context->gate->condition, &context->gate->mutex);
  pthread_mutex_unlock(&context->gate->mutex);

  context->success = run_worker(context->options, context->thread_index);
  return NULL;
}

int run_stress_test(const Options *options) {
  if (options->slots > SIZE_MAX || options->max_size > SIZE_MAX ||
      options->min_size > SIZE_MAX) {
    fprintf(stderr, "Test sizes exceed this platform's size_t range\n");
    return EXIT_FAILURE;
  }

  if (options->threads == 1)
    return run_worker(options, 0) ? EXIT_SUCCESS : EXIT_FAILURE;

  pthread_t *threads = calloc(options->threads, sizeof *threads);
  worker_thread_context *contexts = calloc(options->threads, sizeof *contexts);
  if (threads == NULL || contexts == NULL) {
    fprintf(stderr, "Could not allocate worker thread state\n");
    free(contexts);
    free(threads);
    return EXIT_FAILURE;
  }

  worker_start_gate gate = {
      .mutex = PTHREAD_MUTEX_INITIALIZER,
      .condition = PTHREAD_COND_INITIALIZER,
  };
  unsigned started = 0;
  bool success = true;

  for (unsigned i = 0; i < options->threads; ++i) {
    contexts[i] = (worker_thread_context){
        .options = options,
        .thread_index = i,
        .gate = &gate,
    };
    int error =
        pthread_create(&threads[i], NULL, run_worker_thread, &contexts[i]);
    if (error != 0) {
      fprintf(stderr, "Could not create worker thread %u: %s\n", i,
              strerror(error));
      success = false;
      break;
    }
    ++started;
  }

  pthread_mutex_lock(&gate.mutex);
  gate.start = true;
  pthread_cond_broadcast(&gate.condition);
  pthread_mutex_unlock(&gate.mutex);

  for (unsigned i = 0; i < started; ++i) {
    int error = pthread_join(threads[i], NULL);
    if (error != 0) {
      fprintf(stderr, "Could not join worker thread %u: %s\n", i,
              strerror(error));
      success = false;
    } else if (!contexts[i].success) {
      success = false;
    }
  }

  pthread_cond_destroy(&gate.condition);
  pthread_mutex_destroy(&gate.mutex);
  free(contexts);
  free(threads);

  if (options->output == OUTPUT_JSON) {
    printf("{\"event\":\"aggregate\",\"success\":%s,\"threads\":%u,"
           "\"seed\":%llu}\n",
           success ? "true" : "false", started,
           (unsigned long long)options->seed);
    fflush(stdout);
  } else {
    printf("threads=%u success=%s seed=%llu\n", started,
           success ? "true" : "false", (unsigned long long)options->seed);
  }

  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

int main(int argc, char **argv) {
  Options options = options_default();

  if (!parse_options(argc, argv, &options)) {
    usage(stderr, argv[0]);
    return EXIT_FAILURE;
  }

  if (options.output == OUTPUT_HUMAN) {
    printf("Using allocator: %s\n", options.allocator->name);
    printf("Seed: %llu\n", (unsigned long long)options.seed);
  }

  return run_stress_test(&options);
}
