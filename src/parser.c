
#include "common.h"
#include "parser.h"
#include "parser_p.h"

int generic_parser(const char * const file,
                   ConfigKeywords *config_keywords)
{
    char *linepnt;
    char *linepnt2;
    FILE *fp;
    ConfigKeywords *config_keywords_pnt;
    char line[LINE_MAX];    
    
    if (file == NULL || (fp = fopen(file, "r")) == NULL) {
        return -1;
    }
    while (fgets(line, sizeof line, fp) != NULL) {
        line[sizeof line - 1U] = 0;
        linepnt = line;
        while (*linepnt != 0 && isspace((unsigned char) *linepnt)) {
            linepnt++;
        }
        if (iscntrl((unsigned char) *linepnt) || *linepnt == CONFIG_COMMENT) {
            continue;
        }
        config_keywords_pnt = config_keywords;
        do {
            size_t keyword_len;
            
            keyword_len = strlen(config_keywords_pnt->keyword);
            if (strncasecmp(config_keywords_pnt->keyword,
                            linepnt, keyword_len) == 0) {
                linepnt += keyword_len;
                while (*linepnt != 0 && isspace((unsigned char) *linepnt)) {
                    linepnt++;
                }
                if (*linepnt == 0) {
                    fclose(fp);
                    return -1;
                }
                linepnt2 = linepnt + strlen(linepnt) - 1U;
                while (linepnt2 != linepnt &&
                       (isspace((unsigned char) *linepnt2) || 
                        iscntrl((unsigned char) *linepnt2))) {
                    linepnt2--;
                }
                linepnt2[1] = 0;
                if ((*config_keywords_pnt->value = strdup(linepnt)) == NULL) {
                    fclose(fp);
                    return -1;
                }
                break;
            }
            config_keywords_pnt++;
        } while (config_keywords_pnt->keyword != NULL);
        if (config_keywords_pnt->keyword == NULL) {
            fclose(fp);
            return -1;
        }
    }
    fclose(fp);    
    
    return 0;
}
