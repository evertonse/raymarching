#define STB_C_LEXER_IMPLEMENTATION
#ifndef STB_C_LEXER_DEFINITIONS
// to change the default parsing rules, copy the following lines
// into your C/C++ file *before* including this, and then replace
// the Y's with N's for the ones you don't want. This needs to be
// set to the same values for every place in your program where
// stb_c_lexer.h is included.
// --BEGIN--

#if defined(Y) || defined(N)
#error "Can only use stb_c_lexer in contexts where the preprocessor symbols 'Y' and 'N' are not defined"
#endif

#define STB_C_LEX_C_DECIMAL_INTS    Y   //  "0|[1-9][0-9]*"                        CLEX_intlit
#define STB_C_LEX_C_HEX_INTS        Y   //  "0x[0-9a-fA-F]+"                       CLEX_intlit
#define STB_C_LEX_C_OCTAL_INTS      Y   //  "[0-7]+"                               CLEX_intlit
#define STB_C_LEX_C_DECIMAL_FLOATS  Y   //  "[0-9]*(.[0-9]*([eE][-+]?[0-9]+)?)     CLEX_floatlit
#define STB_C_LEX_C99_HEX_FLOATS    N   //  "0x{hex}+(.{hex}*)?[pP][-+]?{hex}+     CLEX_floatlit
#define STB_C_LEX_C_IDENTIFIERS     Y   //  "[_a-zA-Z][_a-zA-Z0-9]*"               CLEX_id
#define STB_C_LEX_C_DQ_STRINGS      Y   //  double-quote-delimited strings with escapes  CLEX_dqstring
#define STB_C_LEX_C_SQ_STRINGS      N   //  single-quote-delimited strings with escapes  CLEX_ssstring
#define STB_C_LEX_C_CHARS           Y   //  single-quote-delimited character with escape CLEX_charlits
#define STB_C_LEX_C_COMMENTS        Y   //  "/* comment */"
#define STB_C_LEX_CPP_COMMENTS      Y   //  "// comment to end of line\n"
#define STB_C_LEX_C_COMPARISONS     Y   //  "==" CLEX_eq  "!=" CLEX_noteq   "<=" CLEX_lesseq  ">=" CLEX_greatereq
#define STB_C_LEX_C_LOGICAL         Y   //  "&&"  CLEX_andand   "||"  CLEX_oror
#define STB_C_LEX_C_SHIFTS          Y   //  "<<"  CLEX_shl      ">>"  CLEX_shr
#define STB_C_LEX_C_INCREMENTS      Y   //  "++"  CLEX_plusplus "--"  CLEX_minusminus
#define STB_C_LEX_C_ARROW           Y   //  "->"  CLEX_arrow
#define STB_C_LEX_EQUAL_ARROW       N   //  "=>"  CLEX_eqarrow
#define STB_C_LEX_C_BITWISEEQ       Y   //  "&="  CLEX_andeq    "|="  CLEX_oreq     "^="  CLEX_xoreq
#define STB_C_LEX_C_ARITHEQ         Y   //  "+="  CLEX_pluseq   "-="  CLEX_minuseq
                                        //  "*="  CLEX_muleq    "/="  CLEX_diveq    "%=" CLEX_modeq
                                        //  if both STB_C_LEX_SHIFTS & STB_C_LEX_ARITHEQ:
                                        //                      "<<=" CLEX_shleq    ">>=" CLEX_shreq

#define STB_C_LEX_PARSE_SUFFIXES    N   // letters after numbers are parsed as part of those numbers, and must be in suffix list below
#define STB_C_LEX_DECIMAL_SUFFIXES  ""  // decimal integer suffixes e.g. "uUlL" -- these are returned as-is in string storage
#define STB_C_LEX_HEX_SUFFIXES      ""  // e.g. "uUlL"
#define STB_C_LEX_OCTAL_SUFFIXES    ""  // e.g. "uUlL"
#define STB_C_LEX_FLOAT_SUFFIXES    ""  //

#define STB_C_LEX_0_IS_EOF             N  // if Y, ends parsing at '\0'; if N, returns '\0' as token
#define STB_C_LEX_INTEGERS_AS_DOUBLES  N  // parses integers as doubles so they can be larger than 'int', but only if STB_C_LEX_STDLIB==N
#define STB_C_LEX_MULTILINE_DSTRINGS   N  // allow newlines in double-quoted strings
#define STB_C_LEX_MULTILINE_SSTRINGS   N  // allow newlines in single-quoted strings
#define STB_C_LEX_USE_STDLIB           Y  // use strtod,strtol for parsing #s; otherwise inaccurate hack
#define STB_C_LEX_DOLLAR_IDENTIFIER    Y  // allow $ as an identifier character
#define STB_C_LEX_FLOAT_NO_DECIMAL     Y  // allow floats that have no decimal point if they have an exponent

#define STB_C_LEX_DEFINE_ALL_TOKEN_NAMES  N   // if Y, all CLEX_ token names are defined, even if never returned
                                              // leaving it as N should help you catch config bugs

#define STB_C_LEX_DISCARD_PREPROCESSOR    N   // discard C-preprocessor directives (e.g. after prepocess
                                              // still have #line, #pragma, etc)

//#define STB_C_LEX_ISWHITE(str)    ... // return length in bytes of whitespace characters if first char is whitespace

#define STB_C_LEXER_DEFINITIONS         // This line prevents the header file from replacing your definitions
// --END--
#endif
#include "stb/stb_c_lexer.h"

const char* token2string(i64 token);

const char* token2string(i64 token) {
   switch (token) {
   case CLEX_eof:
      return "CLEX_eof";
   case CLEX_parse_error:
      return "CLEX_parse_error";
   case CLEX_intlit:
      return "CLEX_intlit";
   case CLEX_floatlit:
      return "CLEX_floatlit";
   case CLEX_id:
      return "CLEX_id";
   case CLEX_dqstring:
      return "CLEX_dqstring";
   case CLEX_sqstring:
      return "CLEX_sqstring";
   case CLEX_charlit:
      return "CLEX_charlit";
   case CLEX_eq:
      return "CLEX_eq";
   case CLEX_noteq:
      return "CLEX_noteq";
   case CLEX_lesseq:
      return "CLEX_lesseq";
   case CLEX_greatereq:
      return "CLEX_greatereq";
   case CLEX_andand:
      return "CLEX_andand";
   case CLEX_oror:
      return "CLEX_oror";
   case CLEX_shl:
      return "CLEX_shl";
   case CLEX_shr:
      return "CLEX_shr";
   case CLEX_plusplus:
      return "CLEX_plusplus";
   case CLEX_minusminus:
      return "CLEX_minusminus";
   case CLEX_pluseq:
      return "CLEX_pluseq";
   case CLEX_minuseq:
      return "CLEX_minuseq";
   case CLEX_muleq:
      return "CLEX_muleq";
   case CLEX_diveq:
      return "CLEX_diveq";
   case CLEX_modeq:
      return "CLEX_modeq";
   case CLEX_andeq:
      return "CLEX_andeq";
   case CLEX_oreq:
      return "CLEX_oreq";
   case CLEX_xoreq:
      return "CLEX_xoreq";
   case CLEX_arrow:
      return "CLEX_arrow";
   case CLEX_eqarrow:
      return "CLEX_eqarrow";
   case CLEX_shleq:
      return "CLEX_shleq";
   case CLEX_shreq:
      return "CLEX_shreq";
   case CLEX_first_unused_token:
      return "CLEX_first_unused_token";
   default:
      static char store[512];
      if (token >= 0 && token < 256) {
          sprintf(store, "CLEX_token_'%c'", (u8)token);
          return store;
      } else {
          // sprintf(store, "unknown token %lld", token)
          printf("unknown token %lld", token);
          exit(1);
      }
   }
   assert_msg(0, "Damn we shouldn't get here");
}
