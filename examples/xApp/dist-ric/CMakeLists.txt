# Find SQLite3 package
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)


add_executable(dist_ric_xapp 
               dist_ric_xapp.c
              ../../../../src/util/alg_ds/alg/defer.c
            )            

target_link_libraries(dist_ric_xapp
        PUBLIC
        e42_xapp
        -pthread
        -lsctp
        -ldl
)

