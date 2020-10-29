#!/bin/sh
SCRIPT_DOC=/usr/script/
BIN_DOC=/userdata/bin/
PATTERN_DOC=/userdata/pattern/
VOICE_DOC=/userdata/media/voice/
UPDATE_DOC=/userdata/update/


UPDATE_FILE=/userdata/update/update.tar.gz
MPP_RUNFILE=HxtMppService
DESK_RUNFILE=HxtDeskService
START_UP=startup.sh
DETECT_WK=detect.wk
MODEL1_WK=model1.wk
MODEL2_WK=model2.wk
MODEL3_WK=model3.wk


pid_mpp=""
pid_desk=""
updatestate=""
mpp_runstate=""
desk_runstate=""
restart_mpp=1
restart_desk=1
need_restart=0

cd /usr/script/
sh hi3516dv300.sh

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
    sleep 5

    if [ -z "$pid_desk" ]; then
        echo "HxtDeskService not running"
        restart_desk=1
    fi 
    if [ $restart_desk = 1 ]; then
        cd /userdata/bin/
        ./HxtDeskService &
        pid_desk=$!
        restart_desk=0
    fi 

    updatestate=$(ls $UPDATE_FILE)
    if [ -z "$updatestate" ]; then
        sleep 5
    else
        echo "File to update ..."
        cd $UPDATE_DOC
        tar -zxvf $UPDATE_FILE
        if [ $? = 0 ]; then
            if [ -f "$MPP_RUNFILE" ]; then
                kill $pid_mpp
                sleep 1
                cp $MPP_RUNFILE $BIN_DOC
                rm $MPP_RUNFILE
                restart_mpp=1
                need_restart=1
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

            if [ -f "$START_UP" ]; then
                cp $START_UP $SCRIPT_DOC
                rm $START_UP
                need_restart=1
                echo "Starup update complete !!!"
            fi

            wk_files=$(ls *.wk 2>/dev/null | wc -l)
            if [ "$wk_files" != "0" ]; then
                mv *.wk $PATTERN_DOC
                echo "pattern files update complete !!!"
            fi

            voice_files=$(ls *.mp3 2>/dev/null | wc -l)
            if [ "$voice_files" != "0" ]; then
                mv *.mp3 $VOICE_DOC
                echo "voice files update complete !!!"
            fi

            rm $UPDATE_FILE
        fi
    fi
    
    if [ $need_restart = 1 ]; then
        need_start=0
        reboot
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
