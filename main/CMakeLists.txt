idf_component_register(
    SRCS "switchloop.c" "sysloop.c" "mainloop.c" "main.c"
    INCLUDE_DIRS "."
    REQUIRES device core x3switch
)
