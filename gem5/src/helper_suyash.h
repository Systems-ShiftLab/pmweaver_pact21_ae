#ifndef HELPER_SUYASH_HEADER__
#define HELPER_SUYASH_HEADER__

/*--- ANSI text formatting sequences  ---*/
//Regular text
#define BLK "\e[0;30m"
#define RED "\e[0;31m"
#define GRN "\e[0;32m"
#define YEL "\e[0;33m"
#define BLU "\e[0;34m"
#define MAG "\e[0;35m"
#define CYN "\e[0;36m"
#define WHT "\e[0;37m"

//Regular bold text
#define BBLK "\e[1;30m"
#define BRED "\e[1;31m"
#define BGRN "\e[1;32m"
#define BYEL "\e[1;33m"
#define BBLU "\e[1;34m"
#define BMAG "\e[1;35m"
#define BCYN "\e[1;36m"
#define BWHT "\e[1;37m"

//Regular underline text
#define UBLK "\e[4;30m"
#define URED "\e[4;31m"
#define UGRN "\e[4;32m"
#define UYEL "\e[4;33m"
#define UBLU "\e[4;34m"
#define UMAG "\e[4;35m"
#define UCYN "\e[4;36m"
#define UWHT "\e[4;37m"

//Regular background
#define BLKB "\e[40m"
#define REDB "\e[41m"
#define GRNB "\e[42m"
#define YELB "\e[43m"
#define BLUB "\e[44m"
#define MAGB "\e[45m"
#define CYNB "\e[46m"
#define WHTB "\e[47m"

//High intensty background 
#define BLKHB "\e[0;100m"
#define REDHB "\e[0;101m"
#define GRNHB "\e[0;102m"
#define YELHB "\e[0;103m"
#define BLUHB "\e[0;104m"
#define MAGHB "\e[0;105m"
#define CYNHB "\e[0;106m"
#define WHTHB "\e[0;107m"

//High intensty text
#define HBLK "\e[0;90m"
#define HRED "\e[0;91m"
#define HGRN "\e[0;92m"
#define HYEL "\e[0;93m"
#define HBLU "\e[0;94m"
#define HMAG "\e[0;95m"
#define HCYN "\e[0;96m"
#define HWHT "\e[0;97m"

//Bold high intensity text
#define BHBLK "\e[1;90m"
#define BHRED "\e[1;91m"
#define BHGRN "\e[1;92m"
#define BHYEL "\e[1;93m"
#define BHBLU "\e[1;94m"
#define BHMAG "\e[1;95m"
#define BHCYN "\e[1;96m"
#define BHWHT "\e[1;97m"

//Reset
#define RST "\e[0m"

/* Enable errors by default */
#define ALL_SUYASH__
// #define ALL_SUYASH__

#define dbg_var(var)                                                                            \
    #var << var

#if defined(__cplusplus)
    #include <cstdio>
#endif

/* Enable unimplemented function error by default */
#define UNIMPLEMENTED_WARNING_SUYASH__

#if defined(INFO_SUYASH__) && 0 || defined(ALL_SUYASH__) && 0
    #define info__(msg, ...)                                                                        \
    do {fprintf(stderr, GRN "[    INFO] " HYEL "%s" RST ":" HBLU "%d" RST ":" HCYN "%s()" RST       \
                " :: " msg "\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__ );                 \
    } while (0); 
#else
    #define info__(...)                                                                             \
    do {} while(0);
#endif // defined(INFO_SUYASH__) || defined(ALL_SUYASH__)

#if defined(WARN_SUYASH__) && 0 || defined(ALL_SUYASH__) && 0
    #define warn__(msg, ...)                                                                        \
    do {fprintf(stderr, RED "[ WARNING] " HYEL "%s" RST ":" HBLU "%d" RST ":" HCYN "%s()" RST       \
                " :: " msg "\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__ );                 \
    } while (0); 
#else
    #define warn__(...)                                                                             \
    do {} while(0);
#endif // defined(WARN_SUYASH__) || defined(ALL_SUYASH__)

#if defined(ERROR_SUYASH__) || defined(ALL_SUYASH__)
    #define error__(msg, ...)                                                                       \
    do {fprintf(stderr, RED "[   ERROR] " HYEL "%s" RST ":" HBLU "%d" RST ":" HCYN "%s()" RST       \
                " :: " msg "\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__ );                 \
        exit(EXIT_FAILURE);                                                                         \
    } while (0); 
#else
    #define error__(...)                                                                            \
    do {} while(0);
#endif // defined(ERROR_SUYASH__) || defined(ALL_SUYASH__)

#if defined(UNIMPLEMENTED_WARNING_SUYASH__) || defined(ALL_SUYASH__)
    #define unimplemented__(msg, ...)                                                               \
    do {fprintf(stderr, RED "[ UN-IMPL] " HYEL "%s" RST ":" HBLU "%d" RST ":" HCYN "%s()" RST       \
                " :: " msg "\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__ );                 \
        exit(EXIT_FAILURE);                                                                         \
    } while (0); 
#else
    #define unimplemented__(msg, ...)                                                               \
    do {} while(0);
#endif // defined(UNIMPLEMENTED_WARNING_SUYASH__) || defined(ALL_SUYASH__)

#endif // HELPER_SUYASH_HEADER__

#ifdef DISABLE_HELPER_SUYASH__
    #ifdef INFO_SUYASH__
        #undef INFO_SUYASH__
    #endif

    #ifdef info__
        #undef info__
        #define info__(...) do {} while(0);
    #endif

    #ifdef ERROR_SUYASH__
        #undef ERROR_SUYASH__
    #endif

    #ifdef error__
        #undef error__
        #define error__(...) do {} while(0);
    #endif

    #ifdef WARN_SUYASH__
        #undef WARN_SUYASH__
    #endif

    #ifdef warn__
        #undef warn__
        #define warn__(...) do {} while(0);
    #endif

    #ifdef ALL_SUYASH__
        #undef ALL_SUYASH__
    #endif
    
    #undef DISABLE_HELPER_SUYASH__
#endif // DISABLE_HELPER_SUYASH__

