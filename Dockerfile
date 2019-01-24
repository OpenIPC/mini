FROM openwrt

ARG MAKE_ARGS=-j7
ARG MAKE_ARGS="-j1 V=s"

COPY ./minihttp /src/chaos_calmer/camfeed/minihttp
RUN chmod -R 777 /src/chaos_calmer/camfeed/minihttp
RUN ls /src/chaos_calmer/camfeed/minihttp

RUN    printf "\nsrc-link camfeed /src/chaos_calmer/camfeed\n" >> ./feeds.conf \
    && ./scripts/feeds update camfeed \
    && printf "\nCONFIG_PACKAGE_minihttp=y\n" >> .config

RUN    ./scripts/feeds install minihttp \
    && make ${MAKE_ARGS} package/minihttp/compile \
    && rm copy.sh \
    && printf "#!/bin/bash\n\
    set -e\n\
    cp /src/chaos_calmer/build_dir/target-arm_arm926ej-s_uClibc-0.9.33.2_eabi/minihttp-0.1/ipkg-install/usr/bin/minihttp /output/" >> copy.sh \
    && chmod 777 copy.sh

# RUN /src/chaos_calmer/staging_dir/host/bin/sstrip /src/chaos_calmer/build_dir/target-arm_arm926ej-s_uClibc-0.9.33.2_eabi/minihttp-0.1/ipkg-install/usr/bin/minihttp
# RUN upx --best /src/chaos_calmer/build_dir/target-arm_arm926ej-s_uClibc-0.9.33.2_eabi/minihttp-0.1/ipkg-install/usr/bin/minihttp
