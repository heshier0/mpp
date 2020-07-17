#ifndef __IFLYOS_DEFINE_H__
#define __IFLYOS_DEFINE_H__


/****request define****/
//recognizer
#define recog_audion_in          "recognizer.audio_in"
#define recog_text_in            "recognizer.text_in" 
//system
#define sys_state_sync           "system.state_sync"
#define sys_exception            "system.exception"
#define sys_sw_update_result     "system.check_software_update_result"
#define sys_update_sw_state      "system.update_software_state_sync"
//audio player
#define aplayer_pl_prog_sync     "audio_player.playback.progress_sync"
#define aplayer_tts_prog_sync    "audio_player.tts.progress_sync"
#define aplayer_ring_prog_sync   "audio_player.ring.progress_sync"
#define aplayer_tts_text_in      "audio_player.tts.text_in"
//local alarm
#define alarm_state_sync         "alarm.state_sync"
//video player
#define vplayer_prog_sync        "vedio_player.progress_sync"
//playback controller
#define pc_ctrl_cmd              "playback_controller.control_command"
//app action
#define app_check_result         "app_action.check_result"
#define app_exec_success         "app_action.execute_succeed"
#define app_exec_fail            "app_action.execute_failed"
//template
#define tmpl_elem_sel            "template.element_selected"
//launcher
#define launch_sa_result         "launcher.start_activity_result"
#define launch_back_result       "launcher.back_result"
#define launch_sel_result        "launcher.select_result"
//wakeword
#define ww_result                "wakeword.set_wakeword_result"

/*****response define*****/
//recognizer
#define recog_intermediate_text      "recognizer.intermediate_text"
#define recog_stop_capture           "recognizer.stop_capture"
#define recog_expect_reply           "recognizer.expect_reply"
#define recog_evaluate_result        "evaluate_result"
//system
#define sys_ping                     "system.ping"
#define sys_error                    "system.error"
#define sys_chk_sw_update            "system.check_software_update"
#define sys_update_sw                "system.update_software"
#define sys_power_off                "system.power_off"
#define sys_update_dev_modes         "system.update_device_modes"
#define sys_factory_rst              "system.factory_reset"
#define sys_reboot                   "system.reboot"
#define sys_revoke_response          "system.revoke_response"
#define sys_update_alarm_list        "system.update_cloud_alarm_list"
//audio player
#define aplayer_audio_out            "audio_player.audio_out"
//local alarm
#define alarm_set_alarm              "alarm.set_alarm"
#define alarm_delete_alarm           "alarm.delete_alarm"
//speaker
#define spk_set_volume               "speaker.set_volume"
//video player
#define vplayer_video_out            "video_player.video_out"
//app action
#define app_excute                   "app_action.excute"
#define app_check                    "app_action.check"
//screen control
#define screen_set_state             "screen.set_state"
#define screen_set_bright            "screen.set_brightness"
//template
#define tmpl_static                  "template.static_template"
#define tmpl_playing                 "template.playing_template"
#define tmpl_custom                  "template.custom_template"
#define tmpl_exit                    "template.exit"
//launcher
#define launch_sa                    "launcher.start_activity"
#define launch_back                  "launcher.back"
#define launch_sel                   "launcher.select"
//wake word
#define ww_set_ww                    "wakeword.set_wakeword"
//interceptor
#define itcpt_custom                 "interceptor.custom"
#define itcpt_trans_sema             "interceptor.transfer_semantic"



#endif // __IFLYOS_DEFINE_H__



