set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include_directories(include)

add_library(autocancel STATIC SHARED
        include/hashMap.h
        include/cancellable.h
        include/iostat.h
        include/global.h
        include/stat.h
        include/utils.h
        include/shmht.h
        src/cancellable.c
        src/iostat.c
        src/stat.c
        src/hashMap.c
        src/utils.c
        src/shmht.c
        )


target_link_libraries(autocancel
        Threads::Threads
        ${GLIB_LIBRARIES}
        )