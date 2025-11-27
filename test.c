#include <stdio.h>
#include <string.h>

void parse_fstring(const char *input)
{
    char format[1024] = "";
    char exprs[10][256];
    int expr_count = 0;

    int i = 0;
    while (input[i])
    {
        if (input[i] == '{')
        {
            i++;
            char expr[256] = "";
            int j = 0;

            while (input[i] != '}' && input[i] != 0)
            {
                expr[j++] = input[i++];
            }
            expr[j] = 0;
            i++; // skip ending '}'

            strcpy(exprs[expr_count++], expr);

            // Here your compiler would infer type of expr
            // For demo purposes assume all are %s
            strcat(format, "%s");
        }
        else
        {
            int len = strlen(format);
            format[len] = input[i];
            format[len + 1] = 0;
            i++;
        }
    }

    printf("Generated format string: %s\n", format);
    printf("Expressions found:\n");
    for (int k = 0; k < expr_count; k++)
    {
        printf("  expr[%d] = %s\n", k, exprs[k]);
    }
}

int main()
{
    parse_fstring("Hello {name}, your score is {score + 10}");
}