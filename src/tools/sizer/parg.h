/*
 * parg - parse argv
 *
 * Written in 2015-2016 by Joergen Ibsen
 *
 * To the extent possible under law, the author(s) have dedicated all
 * copyright and related and neighboring rights to this software to the
 * public domain worldwide. This software is distributed without any
 * warranty. <http://creativecommons.org/publicdomain/zero/1.0/>
 */

#ifndef PARG_H_INCLUDED
#define PARG_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#define PARG_VER_MAJOR 1        /**< Major version number */
#define PARG_VER_MINOR 0        /**< Minor version number */
#define PARG_VER_PATCH 2        /**< Patch version number */
#define PARG_VER_STRING "1.0.2" /**< Version number as a string */

/**
 * Structure containing state between calls to parser.
 *
 * @see parg_init
 */
struct parg_state {
	const char *optarg;   /**< Pointer to option argument, if any */
	int optind;           /**< Next index in argv to process */
	int optopt;           /**< Option value resulting in error, if any */
	const char *nextchar; /**< Next character to process */
};

/**
 * Structure for supplying long options to `parg_getopt_long()`.
 *
 * @see parg_getopt_long
 */
struct parg_option {
	const char *name; /**< Name of option */
	int has_arg;      /**< Option argument status */
	int *flag;        /**< Pointer to flag variable */
	int val;          /**< Value of option */
};

/**
 * Values for `has_arg` flag in `parg_option`.
 *
 * @see parg_option
 */
typedef enum {
	PARG_NOARG,  /**< No argument */
	PARG_REQARG, /**< Required argument */
	PARG_OPTARG  /**< Optional argument */
} parg_arg_num;

/**
 * Initialize `ps`.
 *
 * Must be called before using state with a parser.
 *
 * @see parg_state
 *
 * @param ps pointer to state
 */
void
parg_init(struct parg_state *ps);

/**
 * Parse next short option in `argv`.
 *
 * Elements in `argv` that contain short options start with a single dash
 * followed by one or more option characters, and optionally an option
 * argument for the last option character. Examples are '`-d`', '`-ofile`',
 * and '`-dofile`'.
 *
 * Consecutive calls to this function match the command-line arguments in
 * `argv` against the short option characters in `optstring`.
 *
 * If an option character in `optstring` is followed by a colon, '`:`', the
 * option requires an argument. If it is followed by two colons, the option
 * may take an optional argument.
 *
 * If a match is found, `optarg` points to the option argument, if any, and
 * the value of the option character is returned.
 *
 * If a match is found, but is missing a required option argument, `optopt`
 * is set to the option character. If the first character in `optstring` is
 * '`:`', then '`:`' is returned, otherwise '`?`' is returned.
 *
 * If no option character in `optstring` matches a short option, `optopt`
 * is set to the option character, and '`?`' is returned.
 *
 * If an element of argv does not contain options (a nonoption element),
 * `optarg` points to the element, and `1` is returned.
 *
 * An element consisting of a single dash, '`-`', is returned as a nonoption.
 *
 * Parsing stops and `-1` is returned, when the end of `argv` is reached, or
 * if an element contains '`--`'.
 *
 * Works similarly to `getopt`, if `optstring` were prefixed by '`-`'.
 *
 * @param ps pointer to state
 * @param argc number of elements in `argv`
 * @param argv array of pointers to command-line arguments
 * @param optstring string containing option characters
 * @return option value on match, `1` on nonoption element, `-1` on end of
 * arguments, '`?`' on unmatched option, '`?`' or '`:`' on option argument
 * error
 */
int
parg_getopt(struct parg_state *ps, int argc, char *const argv[],
            const char *optstring);

/**
 * Parse next long or short option in `argv`.
 *
 * Elements in `argv` that contain a long option start with two dashes
 * followed by a string, and optionally an equal sign and an option argument.
 * Examples are '`--help`' and '`--size=5`'.
 *
 * If no exact match is found, an unambiguous prefix of a long option will
 * match. For example, if '`foo`' and '`foobar`' are valid long options, then
 * '`--fo`' is ambiguous and will not match, '`--foo`' matches exactly, and
 * '`--foob`' is an unambiguous prefix and will match.
 *
 * If a long option match is found, and `flag` is `NULL`, `val` is returned.
 *
 * If a long option match is found, and `flag` is not `NULL`, `val` is stored
 * in the variable `flag` points to, and `0` is returned.
 *
 * If a long option match is found, but is missing a required option argument,
 * or has an option argument even though it takes none, `optopt` is set to
 * `val` if `flag` is `NULL`, and `0` otherwise. If the first character in
 * `optstring` is '`:`', then '`:`' is returned, otherwise '`?`' is returned.
 *
 * If `longindex` is not `NULL`, the index of the entry in `longopts` that
 * matched is stored there.
 *
 * If no long option in `longopts` matches a long option, '`?`' is returned.
 *
 * Handling of nonoptions and short options is like `parg_getopt()`.
 *
 * If no short options are required, an empty string, `""`, should be passed
 * as `optstring`.
 *
 * Works similarly to `getopt_long`, if `optstring` were prefixed by '`-`'.
 *
 * @see parg_getopt
 *
 * @param ps pointer to state
 * @param argc number of elements in `argv`
 * @param argv array of pointers to command-line arguments
 * @param optstring string containing option characters
 * @param longopts array of `parg_option` structures
 * @param longindex pointer to variable to store index of matching option in
 * @return option value on match, `0` for flag option, `1` on nonoption
 * element, `-1` on end of arguments, '`?`' on unmatched or ambiguous option,
 * '`?`' or '`:`' on option argument error
 */
int
parg_getopt_long(struct parg_state *ps, int argc, char *const argv[],
                 const char *optstring,
                 const struct parg_option *longopts, int *longindex);

/**
 * Reorder elements of `argv` so options appear first.
 *
 * If there are no long options, `longopts` may be `NULL`.
 *
 * The return value can be used as `argc` parameter for `parg_getopt()` and
 * `parg_getopt_long()`.
 *
 * @param argc number of elements in `argv`
 * @param argv array of pointers to command-line arguments
 * @param optstring string containing option characters
 * @param longopts array of `parg_option` structures
 * @return index of first nonoption in `argv` on success, `-1` on error
 */
int
parg_reorder(int argc, char *argv[],
             const char *optstring,
             const struct parg_option *longopts);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PARG_H_INCLUDED */
