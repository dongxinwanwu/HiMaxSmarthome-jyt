/***********************************************************
*  File: scene_linkage.h
*  Author: nzy
*  Date: 20180723
***********************************************************/
#ifndef __SCENE_LINKAGE_H
    #define __SCENE_LINKAGE_H

    #include "tuya_cloud_types.h"
    #include "tuya_cloud_com_defs.h"
    #include "action.h"

#ifdef __cplusplus
	extern "C" {
#endif

#ifdef  __SCENE_LINKAGE_GLOBALS
    #define __SCENE_LINKAGE_EXT
#else
    #define __SCENE_LINKAGE_EXT extern
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
typedef BYTE_T LINKAGE_RULE_TYPE_T;

//��https://wiki.tuya-inc.com:7799/pages/viewpage.action?pageId=22086410
#define LINKAGE_RULE_LOCAL     		0 
#define LINKAGE_RULE_LAN     		1 

#define RULE_TYPE_LOCAL_2_PROTOCOL(local) ((local) + 1)
#define RULE_TYPE_PROTOCOL_2_LOCAL(pro)	((pro) - 1)	

#ifdef ENABLE_LAN_LINKAGE_MASTER
#define LINKAGE_RULE_NUM     		2 
#else
#define LINKAGE_RULE_NUM     		1 
#endif

/***********************************************************
*************************variable define********************
***********************************************************/

/***********************************************************
*************************function define********************
***********************************************************/

//设置初始化时是否向云端主动同步
//默认是TRUE
VOID scene_linkage_module_set_init_sync_cloud(BOOL_T enable);

/***********************************************************
*  Function: scene_linkage_module_init
*  Input: none
*  Output: none
*  Return: OPERATE_RET
*  Note:
***********************************************************/
__SCENE_LINKAGE_EXT \
OPERATE_RET scene_linkage_module_init(VOID);

/***********************************************************
*  Function: scene_linkage_module_uninit
*  Input: none
*  Output: none
*  Return: none
*  Note:
***********************************************************/
__SCENE_LINKAGE_EXT \
VOID scene_linkage_module_uninit(VOID);

__SCENE_LINKAGE_EXT \
VOID scene_linkage_update_from_server(LINKAGE_RULE_TYPE_T linkage_type, INT_T delaymS);


#if defined(LOCAL_SCENE) && (LOCAL_SCENE==1)
__SCENE_LINKAGE_EXT \
VOID scene_linkage_scene_exe(CHAR_T *sceneId);

//如果场景在本地，就执行且返回OPRT_OK。
//否则返回不存在 OPRT_NOT_FOUND
__SCENE_LINKAGE_EXT \
OPERATE_RET scene_linkage_scene_exe_by_name(const CHAR_T *scene_name);
#endif
/***********************************************************
*  Function: dp_condition_detect
*  Input: id
*  Input: dp_cmd
*  Output: none 
*  Return: OPERATE_RET
***********************************************************/
// Note:
// id:means gwid,if(cid == NULL) then gwid is actual cid
// dp_cmd:{"cid":"xxxxxx",dps:{"1":2,"2":"111"}} or {"devid":"xxxxxx",dps:{"1":2,"2":"111"}}
__SCENE_LINKAGE_EXT \
OPERATE_RET dp_condition_detect(IN CONST CHAR_T *id,IN CONST CHAR_T *dp_cmd, IN CONST CHAR_T *gw_id, IN LINKAGE_RULE_TYPE_T rule_type, IN BOOL_T b_query);



#if defined(ENABLE_LAN_LINKAGE_MASTER) && (ENABLE_LAN_LINKAGE_MASTER==1)

typedef OPERATE_RET (*LAN_ACTION_SET_EXECUTE)(IN CONST CHAR_T *gw_id, IN CONST CHAR_T *dp_cmd, IN BOOL_T check, OUT BOOL_T *is_local);
typedef OPERATE_RET (*LAN_TOGGLE_ACTION_EXECUTE)(IN CONST ACTION_S *action, OUT BOOL_T *is_local);
typedef OPERATE_RET (*LAN_SCENE_EXECUTE)(IN CONST ACTION_S *action, OUT BOOL_T *is_local);
typedef OPERATE_RET (*LAN_LOCAL_SCENE_EXECUTE)(IN CONST ACTION_S *action, OUT BOOL_T *is_local);
typedef OPERATE_RET (*LAN_DPSTEP_ACTION_EXECUTE)(IN CONST ACTION_S *action, OUT BOOL_T *is_local);
typedef OPERATE_RET (*LAN_GROUP_ACTION_EXECUTE)(IN CONST ACTION_S *action, OUT BOOL_T *is_local);

__SCENE_LINKAGE_EXT \
OPERATE_RET scene_linkage_reg_lan_action(IN LAN_ACTION_SET_EXECUTE func);

__SCENE_LINKAGE_EXT \
OPERATE_RET scene_linkage_reg_lan_toggle_action(IN LAN_TOGGLE_ACTION_EXECUTE func);

__SCENE_LINKAGE_EXT \
OPERATE_RET scene_linkage_reg_lan_scene_action(LAN_SCENE_EXECUTE func);

__SCENE_LINKAGE_EXT \
OPERATE_RET scene_linkage_reg_lan_local_scene_action(LAN_LOCAL_SCENE_EXECUTE func);

__SCENE_LINKAGE_EXT \
OPERATE_RET scene_linkage_reg_lan_dpstep_action(IN LAN_DPSTEP_ACTION_EXECUTE func);

__SCENE_LINKAGE_EXT \
OPERATE_RET scene_linkage_reg_lan_group_action(IN LAN_GROUP_ACTION_EXECUTE func);
#endif


#if defined(ENABLE_ALARM) && (ENABLE_ALARM==1)
OPERATE_RET secne_linkage_parse_alarm(IN CHAR_T *alarmed_mode, OUT CHAR_T **id_list);
#endif


//如果条件或动作是网关本身的，cid为网关的虚拟ID，否则为子设备node_id。调用者需要拷贝内容
typedef struct {
  CHAR_T *cid;
  BYTE_T dp_id;
}SCENE_LINKAGE_ON_EXE_COND_S;

typedef struct {
  CHAR_T *cid;
  BYTE_T dp_id;
}SCENE_LINKAGE_ON_EXE_ACT_DP_ISSUE_S;

//当联动执行时，且动作类型是普通的dp点时，会调用回调,
//cond为最后一个触发的条件
//动作如果有多个，会回调多次
typedef VOID (*SCENE_LINKAGE_ON_EXE_DP_ISSUE)(IN CONST SCENE_LINKAGE_ON_EXE_COND_S *cond, IN CONST  SCENE_LINKAGE_ON_EXE_ACT_DP_ISSUE_S *act_dp);

OPERATE_RET scene_linkage_reg_on_exe_dp_issue(IN SCENE_LINKAGE_ON_EXE_DP_ISSUE on_exe_func);

typedef enum {
   LINKAGE_ABILITY_IR_ISSUE,
   LINKAGE_ABILITY_IR_ISSUEVII,
   LINKAGE_ABILITY_END // 该枚举值无意义, 仅用于标识结束
}LINKAGE_ABILITY_EM;


typedef struct LINKAGE_ABLITY_USER_STRU{
    LINKAGE_ABILITY_EM   usr_ab_idx;
    BOOL_T               is_support;
}LINKAGE_ABLITY_USER_STRU;


/*
 * 函数说明:  
 *          修改网关联动支持的联动动作, 支持修改的动作见枚举LINKAGE_ABILITY_EM, 未设置的动作将保持默认值
 * 
 * 注意事项: 
 *          1. 需要在tuya_iot_init函数之前调用这个函数 
 *
 * 使用示例:
 *          //示例一:  设置支持irIssue,  不支持 irIssueVII,  其它的保持默认值
 *          LINKAGE_ABLITY_USER_STRU ab_usr_stru[] = {
 *                {LINKAGE_ABILITY_IR_ISSUE,              TRUE},
 *                {LINKAGE_ABILITY_IR_ISSUEVII,           FALSE},
 *          };
 *          scene_linkage_gw_linkage_ability_user_set(ab_usr_stru, CNTSOF(ab_usr_stru));
 * 
 *          //示例二:  设置支持irIssueVII,  其它的保持默认值
 *          LINKAGE_ABLITY_USER_STRU ab_usr_stru[] = {
 *                {LINKAGE_ABILITY_IR_ISSUEVII,           TRUE},
 *          };
 *          scene_linkage_gw_linkage_ability_user_set(ab_usr_stru, CNTSOF(ab_usr_stru));
 */
VOID scene_linkage_gw_linkage_ability_user_set(LINKAGE_ABLITY_USER_STRU ab_usr_stru[], INT_T arr_size);


UINT_T scene_linkage_lan_node_timeout_get();


typedef struct{
  UINT_T rule_num;
	CHAR_T **rule_id;
}ALL_RULE_ID;



VOID scene_linkage_all_rule_id_free(ALL_RULE_ID* all_rule_id);

/***
 *  函数说明:
 *     获取网关本地所有场景(本地联动或局域网联动)的rule id(包括使能和未使能)
 *  
 *  注意事项:
 *     使用完之后需要调用scene_linkage_all_rule_id_free释放资源
 *  
 *  使用示例:
 *      // 示例一: 获取本地所有本地联动的rule id
 *      OPERATE_RET exe_ret = scene_linkage_load_all_rule_id_from_disk(LINKAGE_RULE_LOCAL, &all_rule_id);
 *      if(exe_ret == OPRT_OK){
 *          INT_T i = 0;
 *          for(i = 0;  i < all_rule_id.rule_num; ++i){
 *              CONST CHAR_T *rule_id = all_rule_id.rule_id[i];
 *              // do something
 *          }     
 *      }
 *      scene_linkage_all_rule_id_free(&all_rule_id);
***/

OPERATE_RET scene_linkage_load_all_rule_id_from_disk(LINKAGE_RULE_TYPE_T rule_type,  ALL_RULE_ID *all_rule_id);


typedef struct{
    CHAR_T rule_id[RULE_ID_LEN + 1];
    CHAR_T name[128];   
    BOOL_T isHaveCond;  // 0 一键执行 ;  1 自动化
}RULE_INFO;


typedef struct{
    UINT_T rule_num;
    LINKAGE_RULE_TYPE_T rule_type;
    RULE_INFO **rule_info;
}ALL_RULE_INFO;


VOID scene_linkage_all_rule_info_free(ALL_RULE_INFO *all_rule_info);


/***
 *  @brief
 *     Obtain rule information for all local scenarios (local linkage or lan linkage) of the gateway
 *  
 *  @note
 *     After use, you need to call the scene_linkage_all_rule_info_free Release Resources
 *  
 *  @example
 *      // Obtain rule information for all local linkage in the local area
 *      OPERATE_RET exe_ret = scene_linkage_load_all_rule_info_from_disk(LINKAGE_RULE_LOCAL, &all_rule_info);
 *      if(exe_ret == OPRT_OK){
 *          INT_T i = 0;
 *          for(i = 0;  i < all_rule_info.rule_num; ++i){
 *              CONST RULE_INFO *rule_info = all_rule_info.rule_info[i];
 *              // do something by using rule_info
 *          }     
 *      }
 *      scene_linkage_all_rule_info_free(&all_rule_info);
***/
OPERATE_RET scene_linkage_load_all_rule_info_from_disk(LINKAGE_RULE_TYPE_T rule_type,  ALL_RULE_INFO *all_rule_info);



/***
 * 
 *  函数说明:
 *     从云端拉取所有网关可以接管的联动规则类型, 请调用scene_linkage_update_from_server, 本接口为了兼容历史版本才保留
 * 
 *  参数说明:
 *      linkage_type: 想要拉取的联动类型
 *      delaymS     : 延时多久启动拉取线程, 单位ms
 * 
 *  返回值说明:
 *      OPRT_OK: 成功启动拉取线程.
 *      其他值:  失败
 * 
 *  注意事项:
 *      请调用scene_linkage_update_from_server, 该接口为了兼容历史版本才保留, 功能和scene_linkage_update_from_server一样
 *  
 *  使用示例:
 *      scene_linkage_force_update_all_from_server(LINKAGE_RULE_LOCAL, 10000);
 * 
***/
OPERATE_RET scene_linkage_force_update_all_from_server(LINKAGE_RULE_TYPE_T linkage_type, INT_T delaymS);



/**
 * @brief Set how long after the gateway is started, the linkage will not be performed
 * 
 * @param t  时间, 单位秒,  需要在tuya_iot_init之后,  tuya_iot_gw_dev_init之前 调用
 * @return OPERATE_RET 
 */
OPERATE_RET scene_linkage_exe_cntl_set_linkage_stop_dura_when_start(TIME_S t);


//VOID scene_linkage_offline_exe_cntl_ut_test();
//VOID scene_linkage_offline_exe_cntl_func_test(CHAR_T *js);

// 删除存储的所有联动数据
VOID scene_linkage_clear_db();

#ifdef __cplusplus
}
#endif
#endif

