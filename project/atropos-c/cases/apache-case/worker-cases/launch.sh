function normal {
    cp httpd_normal.conf /data/software/apache/dist/conf/httpd.conf
    apachectl -k start
    sleep 10
    echo "start normal"

    ./victim.sh > ./normal-$1.log 2>&1 &
    sleep 20

    sleep 110
}

function inter {
    cp httpd.conf /data/software/apache/dist/conf/httpd.conf
    apachectl -k start
    sleep 10
    echo "start normal inter"

    ./victim.sh > ./inter-$1.log 2>&1 &
    sleep 20

    ./noisy.sh > /dev/null 2>&1 &
    ./noisy.sh > /dev/null 2>&1 &
    ./noisy.sh > /dev/null 2>&1 &

    sleep 110
}

function parties {
    cp httpd.conf /data/software/apache/dist/conf/httpd.conf
    apachectl -k start
    sleep 10
    echo "start normal inter"

    ./victim.sh > ./front_1/parties-$1.log 2>&1 &
    sleep 5
    TLIST=$(ps -e -T | grep httpd | awk '{print $2}' | sort -h)
    for T in $TLIST; do (echo "$T") | sudo tee /sys/fs/cgroup/cpuset/hu_front_1/tasks; done
    sleep 15

    ./noisy.sh > /dev/null 2>&1 &
    TLIST=$(ps -e -T | grep httpd | awk '{print $2}' | sort -h)
    for T in $TLIST; do (echo "$T") | sudo tee /sys/fs/cgroup/cpuset/hu_back_1/tasks; done
    ./noisy.sh > /dev/null 2>&1 &
    TLIST=$(ps -e -T | grep httpd | awk '{print $2}' | sort -h)
    for T in $TLIST; do (echo "$T") | sudo tee /sys/fs/cgroup/cpuset/hu_back_2/tasks; done
    ./noisy.sh > /dev/null 2>&1 &
    TLIST=$(ps -e -T | grep httpd | awk '{print $2}' | sort -h)
    for T in $TLIST; do (echo "$T") | sudo tee /sys/fs/cgroup/cpuset/hu_back_3/tasks; done
    sleep 5
    core=$(nproc --all)
    sudo /data/software/parties/parties_for_native.py ./ $core &

    sleep 110
    sudo pkill -f parties_for_native.py
}


cp php_wrapper $PSANDBOX_APACHE_DIR/php/bin/php-wrapper
cp /data/software/apache/php-7.4.23/php.ini-development /data/software/apache/dist/php/php.ini
cp index.html $PSANDBOX_APACHE_DIR/htdocs/

for p in {2..6}; do
    parties $p
    #without_inter $p

    apachectl -k stop
done

<<COMMENT
if [[ $1 == 0 ]]; then
    normal
elif [[ $1 == 1 ]]; then
    for p in {0..3}; do
        for i in {1..6}; do
            interference $i $p
        done
    done
fi
COMMENT