#!/bin/sh
BIN_DOC=/userdata/bin/
UPDATE_DOC=/userdata/update/


UPDATE_FILE=/userdata/update/update.tar.gz
MPP_RUNFILE=HxtMppService
DESK_RUNFILE=HxtDeskService

pid_mpp=""
pid_desk=""
updatestate=""
mpp_runstate=""
desk_runstate=""
restart_mpp=1
restart_desk=1

while :
do
    if [ -z "$pid_mpp" ]; then
        echo "HxtMppService not running"
        restart_mpp=1
    fi

    if [ $restart_mpp = 1 ]; then
        cd /userdata/bin/
        nohup ./HxtMppService > /dev/null 2>&1 &
        pid_mpp=$!
        restart_mpp=0
    fi

    if [ -z "$pid_desk" ]; then
        echo "HxtDeskService not running"
        restart_desk=1
    fi 
    if [ $restart_desk = 1 ]; then
        cd /userdata/bin/
        nohup ./HxtDeskService > /dev/null 2>&1 &
        pid_desk=$!
        restart_desk=0
    fi 

    updatestate=$(ls $UPDATE_FILE)
    if [ -z "$updatestate" ]; then
        sleep 5
    else
        echo "File to updat ..."
        cd $UPDATE_DOC
        tar -zxvf $UPDATE_FILE
        if [ $? = 0 ]; then
            if [ -f "$MPP_RUNFILE" ]; then
                kill $pid_mpp
                sleep 1
                cp $MPP_RUNFILE $BIN_DOC
                rm $MPP_RUNFILE
                restart_mpp=1
                echo "HxtMppService update complete !!!"
            fi

            if [ -f "$DESK_RUNFILE" ]; then
                kill $pid_desk
                sleep 1
                cp $DESK_RUNFILE $BIN_DOC
                rm $DESK_RUNFILE
                restart_desk=1
                echo "HxtDeskService update complete !!!"
            fi

            rm $UPDATE_FILE
        fi
    fi

    mpp_runstate=$(ps | grep HxtMppService | grep -v grep)
    if [ -z "$mpp_runstate" ]; then
        pid_mpp=""
    fi

    desk_runstate=$(ps | grep HxtDeskService | grep -v grep)
    if [ -z "$desk_runstate" ]; then
        pid_desk=""
    fi

    sleep 10    
done
