#include "ndpi_api.h"
#include "../src/lib/third_party/include/ahocorasick.h"
#include "fuzz_common_code.h"

#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "fuzzer/FuzzedDataProvider.h"

extern "C" int ac_domain_match_handler(AC_MATCH_t *m, AC_TEXT_t *txt, AC_REP_t *match);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  FuzzedDataProvider fuzzed_data(data, size);
  u_int16_t i, j, num_iteration, is_added = 0;
  AC_AUTOMATA_t *a;
  void *a2;
  MATCH_CALLBACK_f mc;
  struct ac_stats stats;
  AC_PATTERN_t ac_pattern;
  char *value_dup, *value_added;
  AC_REP_t match;
  AC_TEXT_t ac_input_text;
  u_int16_t protocol_id;
  ndpi_protocol_category_t category;
  ndpi_protocol_breed_t breed;
  u_int32_t unused;

  /* TODO: real string instead of random bytes */

  /* Use both APIs */

  /* Just to have some data */
  if (fuzzed_data.remaining_bytes() < 1024)
    return -1;

  /* To allow memory allocation failures */
  fuzz_set_alloc_callbacks_and_seed(size);

  if (fuzzed_data.ConsumeBool())
    mc = ac_domain_match_handler;
  else
    mc = NULL;

  ac_automata_enable_debug(0);

  a = ac_automata_init(mc);
  if (fuzzed_data.ConsumeBool())
    a2 = ndpi_init_automa();
  else
    a2 = ndpi_init_automa_domain();

  if (fuzzed_data.ConsumeBool())
    ac_automata_feature(a, AC_FEATURE_DEBUG);
  if (fuzzed_data.ConsumeBool())
    ac_automata_feature(a, AC_FEATURE_LC);
  if (fuzzed_data.ConsumeBool())
    ac_automata_feature(a, AC_FEATURE_NO_ROOT_RANGE);

  if (fuzzed_data.ConsumeBool())
    ac_automata_name(a, (char *)fuzzed_data.ConsumeRandomLengthString(32).c_str(),
                     fuzzed_data.ConsumeBool());

  num_iteration = fuzzed_data.ConsumeIntegral<u_int8_t>();
  for (i = 0; i < num_iteration; i++) {
    std::string value = fuzzed_data.ConsumeRandomLengthString(64);

    /* Adding (sometimes) the same string twice, with different "properties" */
    for (j = 0; j < 2; j++) {
      if (j == 1 && fuzzed_data.ConsumeBool())
        continue;

      memset(&ac_pattern, 0, sizeof(ac_pattern));

      value_dup = ndpi_strdup(value.c_str());
      if (!value_dup)
        continue;

      ac_pattern.astring = value_dup;
      ac_pattern.length = strlen(value_dup);
      ac_pattern.rep.number = fuzzed_data.ConsumeIntegral<u_int16_t>();
      ac_pattern.rep.category = 0;
      ac_pattern.rep.breed = 0;
      ac_pattern.rep.level = fuzzed_data.ConsumeIntegralInRange(0, 2);
      ac_pattern.rep.from_start = fuzzed_data.ConsumeBool();
      ac_pattern.rep.at_end = fuzzed_data.ConsumeBool();
      ac_pattern.rep.dot = memchr(value_dup, '.', strlen(value_dup)) != NULL;

      if (ac_automata_add(a, &ac_pattern) != ACERR_SUCCESS) {
        ndpi_free(value_dup);
      } else {
        /* Keep one random string really added */
        if (is_added == 0 && fuzzed_data.ConsumeBool()) {
          value_added = ndpi_strdup(value_dup);
	  if (value_added)
            is_added = 1;
        }
      }
    }

    value_dup = ndpi_strdup(value.c_str());
    if (!value_dup)
      continue;
    if (ndpi_add_string_to_automa(a2, value_dup) != 0)
      ndpi_free(value_dup);
  }

  if (fuzzed_data.ConsumeBool()) {
    ac_automata_finalize(a);
    ndpi_finalize_automa(a2);
  }

  /* "Random" search */
  num_iteration = fuzzed_data.ConsumeIntegral<u_int8_t>();
  for (i = 0; i < num_iteration; i++) {
    std::string value = fuzzed_data.ConsumeRandomLengthString(64);
    value_dup = ndpi_strdup(value.c_str());
    if (!value_dup)
      continue;

    ac_input_text.astring = value_dup;
    ac_input_text.length = strlen(value_dup);
    ac_input_text.option = 0;
    ac_automata_search(a, &ac_input_text, &match);

    ndpi_free(value_dup);

    ndpi_match_string(a2, fuzzed_data.ConsumeBool() ? NULL : (char *)value.c_str());
    ndpi_match_string_value(a2, fuzzed_data.ConsumeBool() ? NULL : (char *)value.c_str(),
                            strlen(value.c_str()), &unused);
    ndpi_match_string_protocol_id(a2, (char *)value.c_str(), strlen(value.c_str()),
                                  &protocol_id, &category, &breed);
  }
  /* Search of an added node */
  if (is_added) {
    ac_input_text.astring = value_added;
    ac_input_text.length = strlen(value_added);
    ac_input_text.option = 0;

    ac_automata_search(a, &ac_input_text, &match);

    ndpi_match_string(a2, value_added);
    ndpi_match_string_value(a2, value_added, strlen(value_added), &unused);
    ndpi_match_string_protocol_id(a2, value_added, strlen(value_added),
                                  &protocol_id, &category, &breed);

    ndpi_free(value_added);
  }

  ac_automata_dump(a, stdout);

  ac_automata_get_stats(a, &stats);

  ac_automata_release(a, 1);
  ndpi_free_automa(a2);

  return 0;
}
