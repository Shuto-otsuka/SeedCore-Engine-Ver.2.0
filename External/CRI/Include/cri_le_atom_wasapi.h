/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2013-2018 CRI Middleware Co., Ltd.
 *
 * Library  : CRI Atom
 * Module   : Library User's Header for WASAPI
 * File     : cri_atom_wasapi.h
 *
 ****************************************************************************/
/*!
 *	\file		cri_atom_wasapi.h
 */

/* 多重定義防止					*/
/* Prevention of redefinition	*/
#ifndef CRI_INCL_CRI_ATOM_WASAPI_H
#define CRI_INCL_CRI_ATOM_WASAPI_H

/***************************************************************************
 *      インクルードファイル
 *      Include files
 ***************************************************************************/
#include <Windows.h>
#include <sdkddkver.h>
#include <Audioclient.h>
#include <AudioSessionTypes.h>
#include <mmdeviceapi.h>
#include <External/CRI/Include/cri_le_xpt.h>
#include <External/CRI/Include/cri_le_atom.h>
#include <External/CRI/Include/cri_le_atom_ex.h>
#include <External/CRI/Include/cri_le_atom_asr.h>

/***************************************************************************
 *      定数マクロ
 *      Macro Constants
 ***************************************************************************/

/***************************************************************************
 *      処理マクロ
 *      Macro Functions
 ***************************************************************************/
/*==========================================================================
 *      CRI Atom API
 *=========================================================================*/
/*JP
 * \brief ライブラリ初期化用コンフィグ構造体にデフォルト値をセット
 * \ingroup ATOMLIB_WASAPI
 * \param[out]	p_config	初期化用コンフィグ構造体へのポインタ
 * \par 説明:
 * ::criAtom_Initialize_WASAPI 関数に設定するコンフィグ構造体
 * （ ::CriAtomConfig_WASAPI ）に、デフォルトの値をセットします。<br>
 * \attention
 * 本マクロは下位レイヤ向けのAPIです。<br>
 * AtomExレイヤの機能を利用する際には、本マクロの代わりに 
 * ::criAtomEx_SetDefaultConfig_WASAPI マクロをご利用ください。
 * \sa CriAtomConfig_WASAPI
 */
#define criAtom_SetDefaultConfig_WASAPI(p_config)			\
{															\
	criAtom_SetDefaultConfig(&(p_config)->atom);			\
	criAtomAsr_SetDefaultConfig(&(p_config)->asr);			\
	criAtomHcaMx_SetDefaultConfig(&(p_config)->hca_mx);		\
}

/*==========================================================================
 *      CRI AtomEx API
 *=========================================================================*/
/*JP
 * \brief ライブラリ初期化用コンフィグ構造体にデフォルト値をセット
 * \ingroup ATOMLIB_WASAPI
 * \param[out]	p_config	初期化用コンフィグ構造体へのポインタ
 * \par 説明:
 * ::criAtomEx_Initialize_WASAPI 関数に設定するコンフィグ構造体
 * （ ::CriAtomExConfig_WASAPI ）に、デフォルトの値をセットします。<br>
 * \sa CriAtomExConfig_WASAPI
 */
#define criAtomEx_SetDefaultConfig_WASAPI(p_config)			\
{															\
	criAtomEx_SetDefaultConfig(&(p_config)->atom_ex);		\
	(p_config)->atom_ex.thread_model = CRIATOMEX_THREAD_MODEL_MULTI_WITH_SONICSYNC; \
	criAtomExAsr_SetDefaultConfig(&(p_config)->asr);		\
	criAtomExHcaMx_SetDefaultConfig(&(p_config)->hca_mx);	\
}

/***************************************************************************
 *      データ型宣言
 *      Data Type Declarations
 ***************************************************************************/
/*==========================================================================
 *      CRI Atom API
 *=========================================================================*/
/*JP
 * \brief Atomライブラリ初期化用コンフィグ構造体
 * \ingroup ATOMLIB_WASAPI
 * CRI Atomライブラリの動作仕様を指定するための構造体です。<br>
 * ::criAtom_Initialize_WASAPI 関数の引数に指定します。<br>
 * \attention
 * 本構造体は下位レイヤ向けのAPIです。<br>
 * AtomExレイヤの機能を利用する際には、本構造体の代わりに 
 * ::CriAtomExConfig_WASAPI 構造体をご利用ください。
 * \sa criAtom_Initialize_WASAPI, criAtom_SetDefaultConfig_WASAPI
 */
typedef struct CriAtomConfigTag_WASAPI {
	CriAtomConfig			atom;		/*JP< Atom初期化用コンフィグ構造体		*/
	CriAtomAsrConfig		asr;		/*JP< ASR初期化用コンフィグ			*/
	CriAtomHcaMxConfig		hca_mx;		/*JP< HCA-MX初期化用コンフィグ構造体	*/
} CriAtomConfig_WASAPI;

/*JP
 * \brief オーディオエンドポイント列挙コールバック
 * \ingroup ATOMLIB_WASAPI
 * \param[in]	object				ユーザ指定オブジェクト
 * \param[in]	device				IMMDeviceインスタンス
 * \par 説明:
 * オーディオエンドポイントの通知に使用される、コールバック関数の型です。<br>
 * ::criAtom_EnumAudioEndpoints_WASAPI 関数に本関数型のコールバック関数を登録することで、
 * IMMDeviceインスタンスをコールバック経由で受け取ることが可能です。<br>
 * \attention
 * IMMDeviceインスタンスをコールバック関数内で破棄してはいけません。<br>
 * \sa criAtom_EnumAudioEndpoints_WASAPI
 */
typedef void (CRIAPI *CriAtomAudioEndpointCbFunc_WASAPI)(void *object, IMMDevice *device);

/*JP
 * \brief デバイス更新通知コールバック
 * \ingroup ATOMLIB_WASAPI
 * \param[in]	object				ユーザ指定オブジェクト
 * \par 説明:
 * デバイスの更新通知に使用される、コールバック関数の型です。<br>
 * ::criAtom_SetDeviceUpdateCallback_WASAPI 関数に本関数型のコールバック関数を登録することで、
 * デバイスが更新された際にコールバック経由で通知を受け取ることが可能です。<br>
 * \sa criAtom_SetDeviceUpdateCallback_WASAPI
 */
typedef void (CRIAPI *CriAtomDeviceUpdateCbFunc_WASAPI)(void *object);

#if defined(XPT_TGT_PC)
/*JP
 * \brief ハプティクスデバイスタイプ
 * \ingroup ATOMLIB_WASAPI
 * Windowsで使用するハプティクスデバイスのタイプです。<br>
 * ::criAtom_SetHapticDeviceId_WASAPI 関数の引数に指定します。<br>
 * CRIATOM_HAPTIC_DEVICE_TYPE_DEFAULT : ステレオ出力を振動に使用するデバイスタイプです。Atomはデバイスにそのままの信号を出力します。
 * CRIATOM_HAPTIC_DEVICE_TYPE_DLS : Ls／Rs成分を振動に使用するデバイスタイプです。Atomは内部で信号のL/R成分をデバイスのLs／Rsに出力します。
 * \sa criAtom_SetHapticDeviceId_WASAPI
 */
typedef enum CriAtomHapticDeviceTypeTag_WASAPI {
	CRIATOM_HAPTIC_DEVICE_TYPE_DEFAULT = 0,
	CRIATOM_HAPTIC_DEVICE_TYPE_DLS,
	CRIATOM_HAPTIC_DEVICE_TYPE_ENUM_SIZE_IS_4BYTES = 0x7FFFFFFF
}CriAtomHapticDeviceType_WASAPI;
#endif
/*==========================================================================
 *      CRI AtomEx API
 *=========================================================================*/
/*JP
 * \brief Atomライブラリ初期化用コンフィグ構造体
 * \ingroup ATOMLIB_WASAPI
 * CRI Atomライブラリの動作仕様を指定するための構造体です。<br>
 * ::criAtomEx_Initialize_WASAPI 関数の引数に指定します。<br>
 * \sa criAtomEx_Initialize_WASAPI, criAtomEx_SetDefaultConfig_WASAPI
 */
typedef struct CriAtomExConfigTag_WASAPI {
	CriAtomExConfig			atom_ex;	/*JP< AtomEx初期化用コンフィグ構造体	*/
	CriAtomExAsrConfig		asr;		/*JP< ASR初期化用コンフィグ			*/
	CriAtomExHcaMxConfig	hca_mx;		/*JP< HCA-MX初期化用コンフィグ構造体	*/
} CriAtomExConfig_WASAPI;

/***************************************************************************
 *      変数宣言
 *      Prototype Variables
 ***************************************************************************/

/***************************************************************************
 *      関数宣言
 *      Prototype Functions
 ***************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

/*==========================================================================
 *      CRI Atom API
 *=========================================================================*/
/*JP
 * \brief ライブラリ初期化用ワーク領域サイズの計算
 * \ingroup ATOMLIB_WASAPI
 * \param[in]	config		初期化用コンフィグ構造体
 * \return		CriSint32	ワーク領域サイズ
 * \par 説明:
 * ライブラリを使用するために必要な、ワーク領域のサイズを取得します。<br>
 * \par 備考:
 * ライブラリが必要とするワーク領域のサイズは、ライブラリ初期化用コンフィグ
 * 構造体（ ::CriAtomConfig_WASAPI ）の内容によって変化します。<br>
 * <br>
 * 引数 config の情報は、関数内でのみ参照されます。<br>
 * 関数を抜けた後は参照されませんので、関数実行後に config の領域を解放しても
 * 問題ありません。
 * \attention
 * 本関数は下位レイヤ向けのAPIです。<br>
 * AtomExレイヤの機能を利用する際には、本関数の代わりに 
 * ::criAtomEx_CalculateWorkSize_WASAPI 関数をご利用ください。
 * \sa CriAtomConfig_WASAPI, criAtom_Initialize_WASAPI
 */
CriSint32 CRIAPI criAtom_CalculateWorkSize_WASAPI(const CriAtomConfig_WASAPI *config);

/*JP
 * \brief ライブラリの初期化
 * \ingroup ATOMLIB_WASAPI
 * \param[in]	config		初期化用コンフィグ構造体
 * \param[in]	work		ワーク領域
 * \param[in]	work_size	ワーク領域サイズ
 * \par 説明:
 * ライブラリを初期化します。<br>
 * ライブラリの機能を利用するには、必ずこの関数を実行する必要があります。<br>
 * （ライブラリの機能は、本関数を実行後、 ::criAtom_Finalize_WASAPI 関数を実行するまでの間、
 * 利用可能です。）<br>
 * <br>
 * ライブラリを初期化する際には、ライブラリが内部で利用するためのメモリ領域（ワーク領域）
 * を確保する必要があります。<br>
 * ライブラリが必要とするワーク領域のサイズは、初期化用コンフィグ構造体の内容に応じて
 * 変化します。<br>
 * ワーク領域サイズの計算には、 ::criAtom_CalculateWorkSize_WASAPI 
 * 関数を使用してください。<br>
 * \par 備考:
 * ::criAtom_SetUserAllocator マクロを使用してアロケーターを登録済みの場合、
 * 本関数にワーク領域を指定する必要はありません。<br>
 * （ work に NULL 、 work_size に 0 を指定することで、登録済みのアロケーター
 * から必要なワーク領域サイズ分のメモリが動的に確保されます。）
 * <br>
 * 引数 config の情報は、関数内でのみ参照されます。<br>
 * 関数を抜けた後は参照されませんので、関数実行後に config の領域を解放しても
 * 問題ありません。
 * \attention
 * 本関数は内部的に以下の関数を実行します。<br>
 * 	- ::criAtom_Initialize
 * 	- ::criAtomAsr_Initialize
 * 	- ::criAtomHcaMx_Initialize
 * 本関数を実行する場合、上記関数を実行しないでください。<br>
 * <br>
 * 本関数を実行後、必ず対になる ::criAtom_Finalize_WASAPI 関数を実行してください。<br>
 * また、 ::criAtom_Finalize_WASAPI 関数を実行するまでは、本関数を再度実行しないでください。<br>
 * <br>
 * 本関数は下位レイヤ向けのAPIです。<br>
 * AtomExレイヤの機能を利用する際には、本関数の代わりに 
 * ::criAtomEx_Initialize_WASAPI 関数をご利用ください。
 * \sa CriAtomConfig_WASAPI, criAtom_Finalize_WASAPI,
 * criAtom_SetUserAllocator, criAtom_CalculateWorkSize_WASAPI
 */
void CRIAPI criAtom_Initialize_WASAPI(
	const CriAtomConfig_WASAPI *config, void *work, CriSint32 work_size);

/*JP
 * \brief ライブラリの終了
 * \ingroup ATOMLIB_WASAPI
 * \par 説明:
 * ライブラリを終了します。<br>
 * \attention
 * 本関数は内部的に以下の関数を実行します。<br>
 * 	- ::criAtom_Finalize
 * 	- ::criAtomAsr_Finalize
 * 	- ::criAtomHcaMx_Finalize
 * 本関数を実行する場合、上記関数を実行しないでください。<br>
 * <br>
 * ::criAtom_Initialize_WASAPI 関数実行前に本関数を実行することはできません。<br>
 * <br>
 * 本関数は下位レイヤ向けのAPIです。<br>
 * AtomExレイヤの機能を利用する際には、本関数の代わりに 
 * ::criAtomEx_Finalize_WASAPI 関数をご利用ください。
 * \sa criAtom_Initialize_WASAPI
 */
void CRIAPI criAtom_Finalize_WASAPI(void);

/*JP
 * \brief デフォルトデバイス種別の指定
 * \ingroup ATOMLIB_WASAPI
 * \param[out]	role		デフォルトデバイスとして使用するデバイスの種別
 * \par 説明:
 * デフォルトデバイスの種別を指定します。
 * roleにeConsoleを指定した場合、Atomライブラリは既定のデバイスを使用して音声を出力します。<br>
 * roleにeCommunicationsを指定した場合、Atomライブラリは既定の通信デバイスを使用して音声を出力します。
 * \attention
 * 本関数はライブラリ初期化前に使用する必要があります。<br>
 */
#if !defined(XPT_TGT_WINRT)
void CRIAPI criAtom_SetDefaultDeviceRole_WASAPI(ERole role);
#endif

/*JP
 * \brief ミキサフォーマットの取得
 * \ingroup ATOMLIB_WASAPI
 * \param[out]	format		ミキサのフォーマット
 * \return		CriBool		ミキサのフォーマットが取得できたかどうか（ CRI_TRUE = 成功、CRI_FALSE = 失敗）
 * \par 説明:
 * 共有モード時に使用されるミキサのフォーマットを取得します。<br>
 * \par 備考:
 * 本関数は IAudioClient::GetMixFormat 関数のラッパーです。<br>
 * 本関数を実行すると、関数内で AudioClient を作成し、GetMixFormat 関数を実行します。<br>
 * \attention
 * 本関数はライブラリ初期化前にのみ使用可能です。<br>
 * <br>
 * 共有モードと排他モードとでは、使用できるフォーマットが異なります。<br>
 * 本関数で取得する WAVEFORMATEXTENSIBLE 構造体は IEEE float 形式のPCMデータフォーマットを返しますが、
 * このフォーマットは排他モードではほとんどの場合使用できません。<br>
 */
CriBool CRIAPI criAtom_GetAudioClientMixFormat_WASAPI(WAVEFORMATEXTENSIBLE *format);

/*JP
 * \brief 指定したフォーマットが利用可能かどうかチェック
 * \ingroup ATOMLIB_WASAPI
 * \param[in]	format		使用するフォーマット
 * \return		CriBool		指定されたフォーマットが利用可能かどうか（ CRI_TRUE = 利用可能、CRI_FALSE = 利用不可能）
 * \par 説明:
 * 引数で指定したフォーマットが、排他モードで利用可能かどうかをチェックします。<br>
 * \par 備考:
 * 本関数は IAudioClient::IsFormatSupported 関数のラッパーです。<br>
 * 本関数を実行すると、関数内で AudioClient を作成し、IsFormatSupported 関数を実行します。<br>
 * \attention
 * 本関数はライブラリ初期化前にのみ使用可能です。<br>
 * <br>
 * 一部のデバイス／パラメーターについて、本関数が成功するにもかかわらず、
 * WASAPI の初期化に失敗するケースが確認されています。<br>
 * 本関数が CRI_TRUE を返したにもかかわらず、ライブラリの初期化に失敗する場合には、
 * 指定するフォーマットを変更するか、または共有モードをご使用ください。<br>
 */
CriBool CRIAPI criAtom_GetAudioClientIsFormatSupported_WASAPI(const WAVEFORMATEX *format);

/*JP
 * \brief 共有方式の指定
 * \ingroup ATOMLIB_WASAPI
 * \param[in]	mode	使用するモード
 * \par 説明:
 * WASAPIを共有モードで使用するか、排他モードで使用するかを指定します。<br>
 * <br>
 * 本関数を実行しない場合や、 AUDCLNT_SHAREMODE_SHARED を指定して実行した場合、
 * Atomライブラリは WASAPI を共有モードで初期化します。<br>
 * AUDCLNT_SHAREMODE_EXCLUSIVE を指定して本関数を実行した場合、
 * Atomライブラリは WASAPI を排他モードで初期化します。<br>
 * \attention
 * 本関数はライブラリの初期化よりも先に実行しておく必要があります。<br>
 * <br>
 * 排他モードを使用する場合、本関数でのモード指定に加え、
 * ::criAtom_SetAudioClientFormat_WASAPI 関数によるフォーマットの指定が必要です。<br>
 * \sa criAtom_SetAudioClientFormat_WASAPI
 */
void CRIAPI criAtom_SetAudioClientShareMode_WASAPI(AUDCLNT_SHAREMODE mode);

/*JP
 * \brief 共有方式の取得
 * \ingroup ATOMLIB_WASAPI
 * \return	AUDCLNT_SHAREMODE	共有方式
 * \par 説明:
 * 現在指定されている共有方式を取得します。
 * \sa criAtom_SetAudioClientShareMode_WASAPI
 */
AUDCLNT_SHAREMODE CRIAPI criAtom_GetAudioClientShareMode_WASAPI(void);

/*JP
 * \brief 出力フォーマットの指定
 * \ingroup ATOMLIB_WASAPI
 * \par 説明:
 * \param[in]	format		使用するフォーマット
 * \par 説明:
 * 排他モードで使用するフォーマットを指定します。<br>
 * \par 備考:
 * 本関数に指定したフォーマットが、 IAudioClient::Initialize 関数に渡されます。<br>
 * \attention
 * 本関数はライブラリの初期化よりも先に実行しておく必要があります。<br>
 * <br>
 * 排他モードを使用する場合、本関数でのモード指定に加え、
 * ::criAtom_SetAudioClientShareMode_WASAPI 関数によるモード指定が必要です。<br>
 * \sa criAtom_SetAudioClientShareMode_WASAPI
 */
void CRIAPI criAtom_SetAudioClientFormat_WASAPI(const WAVEFORMATEX *format);

/*JP
 * \brief バッファリング時間の指定
 * \ingroup ATOMLIB_WASAPI
 * \param[in]	ref_time	バッファリング時間
 * \par 説明:
 * WASAPI 初期化時に指定するバッファリング時間を指定します。<br>
 * Atomライブラリは、本関数で指定された時間分のデータを保持可能なサイズのサウンドバッファーを確保します。<br>
 * \par 備考:
 * 本関数に指定した値が IAudioClient::Initialize 関数に渡されます。<br>
 * <br>
 * ref_timeに0を指定した場合や、本関数を使用しない場合、
 * Atomライブラリは初期化時に指定されるサーバー処理周波数の値から、
 * 適切なバッファリング時間を計算します。<br>
 * <br>
 * PC環境ではハードウェア性能にばらつきがあるため、
 * ワースト性能のハードウェアに合わせてデフォルトのバッファリング量が多めに設定されています。<br>
 * （デフォルト状態では4V分のバッファーを持っています。）
 * \attention
 * 本関数はライブラリの初期化よりも先に実行しておく必要があります。<br>
 * <br>
 * バッファリング時間を小さくしすぎると、音途切れ等の問題が発生します。<br>
 * <br>
 * PC環境ではハードウェアに依存して必要なバッファリング量が異なるため、
 * 本関数を使用した場合、テスト環境でうまく動作していても、
 * ユーザの環境によっては音途切れが発生する可能性があります。<br>
 * そのため、バッファリング量を変更する場合には、
 * ユーザが設定値を変更できる仕組み（オプション画面等）を提供することもご検討ください。
 */
void CRIAPI criAtom_SetAudioClientBufferDuration_WASAPI(REFERENCE_TIME ref_time);

/*JP
 * \brief AudioClientの取得
 * \ingroup ATOMLIB_WASAPI
 * \return IAudioClient	AudioClient
 * \par 説明:
 * Atomライブラリ内で作成されたAudioClientを取得します。
 * \par 備考:
 * サウンドデバイスが搭載されていないPCで本関数を実行した場合、
 * 本関数はNULLを返します。
 * \par 注意:
 * 本関数を実行する前に、ライブラリを初期化する必要があります。
 */
IAudioClient * CRIAPI criAtom_GetAudioClient_WASAPI(void);

/*JP
 * \brief デバイスが無効化されたかどうかのチェック
 * \ingroup ATOMLIB_WASAPI
 * \return	CriBool	デバイスが無効化されたかどうか（CRI_TRUE = 無効化された、CRI_FALSE = 正常に動作中）
 * \par 説明:
 * サウンドデバイスが無効化されたかどうかを返します。
 * \par 備考:
 * 本関数がCRI_TRUEを返すのは、アプリケーション実行中にサウンドデバイスを無効化した場合のみです。<br>
 * 元々サウンドデバイスが搭載されていないPCで本関数を実行した場合、本関数はCRI_FALSEを返します。<br>
 * （サウンドデバイスの有無は別途 ::criAtom_GetAudioClient_WASAPI 関数でチェックする必要があります。）
 * \sa criAtom_GetAudioClient_WASAPI
 */
CriBool CRIAPI criAtom_IsDeviceInvalidated_WASAPI(void);

/*JP
 * \brief サウンドデバイスの指定
 * \ingroup ATOMLIB_WASAPI
 * \param[in]	type		サウンドレンダラタイプ
 * \param[in]	device_id	デバイスID
 * \par 説明:
 * サウンドレンダラタイプとサウンドデバイスの紐づけを行います。<br>
 * <br>
 * 本関数でサウンドレンダラに対してデバイスIDを設定すると、
 * 当該サウンドレンダラを指定して出力した音声は、
 * 全て指定したIDに合致するサウンドデバイスから出力されます。
 * <br>
 * type には、以下の値が指定可能です。<br>
 * 	- CRIATOM_SOUND_RENDERER_HW1（CRIATOM_SOUND_RENDERER_NATIVEと同じ値）
 * 	- CRIATOM_SOUND_RENDERER_HW2
 * 	- CRIATOM_SOUND_RENDERER_HW3
 * 	- CRIATOM_SOUND_RENDERER_HW4
 * 第2引数（device_id）にNULLまたは長さ0の文字列を指定した場合、
 * 当該サウンドレンダラとデバイスIDの紐づけが解除されます。<br>
 * （既定のデバイスから音声を出力するよう、動作が変更されます。）
 * \par 備考:
 * サウンドデバイスのIDは IMMDevice::GetId で取得する必要があります。<br>
 * 指定されたIDに一致するサウンドデバイスが見つからない場合、
 * 当該デバイスを指定して再生された音声は、既定のデバイスから出力されます。<br>
 */
void CRIAPI criAtom_SetDeviceId_WASAPI(CriAtomSoundRendererType type, LPCWSTR device_id);

/*JP
 * \brief サウンドデバイスのID取得
 * \ingroup ATOMLIB_WASAPI
 * \param[in]	type				サウンドレンダラタイプ
 * \param[out]	device_id			デバイスID格納領域
 * \param[in]	count				デバイスID格納領域のサイズ（文字数）
 * \param[out]	is_default_device	デフォルトデバイスかどうか
 * \return		CriBool				デバイスIDが取得できたかどうか
 * \par 説明:
 * サウンドレンダラタイプに紐づけられたサウンドデバイスのIDを取得します。<br>
 * <br>
 * type には、以下の値が指定可能です。<br>
 * 	- CRIATOM_SOUND_RENDERER_HW1（CRIATOM_SOUND_RENDERER_NATIVEと同じ値）
 * 	- CRIATOM_SOUND_RENDERER_HW2
 * 	- CRIATOM_SOUND_RENDERER_HW3
 * 	- CRIATOM_SOUND_RENDERER_HW4
 * <br>
 * 第2引数（device_id）にはデバイスID文字列を受け取るためのメモリ領域を指定します。
 * この領域は本関数を呼び出すアプリケーション側で確保する必要があります。<br>
 * 第3引数（count）には、第2引数で指定したメモリ領域に格納可能な最大文字数を指定します。
 * <br>
 * 指定したサウンドレンダラに対応するデバイスがデフォルトデバイスの場合、
 * 第4引数（is_default_device）には CRI_TRUE がセットされます。<br>
 * 指定したサウンドレンダラに対応するデバイスが ::criAtom_SetDeviceId_WASAPI 関数で
 * 指定されたデバイスの場合、第4引数（is_default_device）には CRI_FALSE がセットされます。<br>
 * <br>
 * サウンドデバイスのIDが取得できた場合、本関数は CRI_TRUE を返します。<br>
 * 本関数が CRI_FALSE を返した場合、以下のいずれかに該当しています。<br>
 * - サウンドデバイスが存在しない
 * - デバイスID文字列を格納するための領域が不足している
 * - プラットフォームSDKがエラーを返した
 * - 不正な引数が指定された
 * サウンドデバイスが存在しない場合以外は合わせてエラーコールバックが通知されます。
 * \attention
 * 第3引数で指定するサイズはバイト数ではなく文字数です。<br>
 * デバイスID文字列は通常ワイド文字列（wchar_t型）であるため、
 * 第2引数で指定するメモリ領域は、文字数に sizeof(wchar_t) を掛けたサイズ分必要です。
 * <br>
 * デフォルトデバイスが使用されている場合（第4引数で取得した値が CRI_TRUE の場合）に、
 * 本関数で取得したデバイスIDを ::criAtom_SetDeviceId_WASAPI 関数に指定する
 * （デフォルトデバイスを取得したデバイスIDで上書きする）と、
 * デフォルトデバイス指定を追従しない形に挙動が変更されてしまいます。<br>
 * 具体的には、デフォルトデバイス使用時はユーザーのWindows上での指定に応じて
 * サウンドデバイスが自動的に切り替わるのに対し、
 * デバイスID上書き後はユーザーがWindows上で既定のデバイスを変更したとしても、
 * デバイスが自動的には変更されない挙動となります。
 * 本関数を使用する際には、ユーザーによる既定のデバイスの変更操作が無効化されないよう、
 * デバイスIDだけでなくデフォルトデバイスかどうかも合わせて確認してください。
 * \sa criAtom_SetDeviceId_WASAPI
 */
CriBool CRIAPI criAtom_GetDeviceId_WASAPI(CriAtomSoundRendererType type,
	LPWSTR device_id, CriSint32 count, CriBool *is_default_device);

/*JP
 * \brief オーディオエンドポイントの列挙
 * \ingroup ATOMLIB_WASAPI
 * \param[in]	callback	オーディオエンドポイントコールバック関数
 * \param[in]	object		ユーザ指定オブジェクト
 * \return		CriSint32	列挙されたACBハンドルの数
 * \par 説明:
 * オーディオエンドポイントを列挙します。<br>
 * <br>
 * 本関数を実行すると、第 1 引数（ callback ）
 * でセットされたコールバック関数がオーディオエンドポイント数分だけ呼び出されます。<br>
 * コールバック関数には、IMMDeviceインスタンスが引数として渡されます。<br>
 * \par 備考:
 * 第 2 引数（ object ）にセットした値は、コールバック関数の引数として渡されます。<br>
 * コールバック関数のその他の引数については、
 * 別途 ::CriAtomAudioEndpointCbFunc_WASAPI の説明をご参照ください。<br>
 * <br>
 * 戻り値は列挙されたオーディオエンドポイントの数（登録したコールバック関数が呼び出された回数）です。<br>
 * オーディオエンドポイントが存在しない場合、本関数は 0 を返します。<br>
 * エラーが発生した際には -1 を返します。<br>
 * \attention
 * IMMDeviceインスタンスをコールバック関数内で破棄してはいけません。<br>
 * \sa CriAtomAudioEndpointCbFunc_WASAPI
 */
CriSint32 CRIAPI criAtom_EnumAudioEndpoints_WASAPI(CriAtomAudioEndpointCbFunc_WASAPI callback, void *object);

/*JP
 * \brief デバイス更新通知の登録
 * \ingroup ATOMLIB_WASAPI
 * \param[in]	callback	デバイス更新通知コールバック関数
 * \param[in]	object		ユーザ指定オブジェクト
 * \par 説明:
 * デバイスの更新通知を受け取るためのコールバックを設定します。<br>
 * <br>
 * 本関数を実行すると、デバイスが更新された際、第 1 引数（ callback ）
 * でセットされたコールバック関数が呼び出されます。<br>
 * \par 備考:
 * 第 2 引数（ object ）にセットした値は、コールバック関数の引数として渡されます。<br>
 * \sa CriAtomAudioEndpointCbFunc_WASAPI
 */
void CRIAPI criAtom_SetDeviceUpdateCallback_WASAPI(CriAtomDeviceUpdateCbFunc_WASAPI callback, void *object);

/*JP
 * \brief スペーシャルオーディオ機能の有効化
 * \ingroup ATOMLIB_WASAPI
 * \param[in]	type		サウンドレンダラタイプ
 * \param[in]	sw			機能を有効にするかどうか（CRI_TRUE = 有効化、CRI_FALSE = 無効化）
 * \par 説明:
 * スペーシャルオーディオ機能（Microsoft Spatial Sound）を有効にします。<br>
 * 引数の type には、スペーシャルオーディオ機能を有効（又は無効）にするサウンドレンダラを指定します。<br>
 * \par 備考:
 * 現行のライブラリでは、スペーシャルオーディオ機能はデフォルトで有効です。<br>
 * そのため、スペーシャルオーディオ機能を無効にしたい場合を除き、本関数を明示的に呼び出す必要はありません。<br>
 */
void CRIAPI criAtom_SetSpatialAudioEnabled_WASAPI(CriAtomSoundRendererType type, CriBool sw);

/*JP
 * \brief スペーシャルオーディオ機能が有効かどうかのチェック
 * \ingroup ATOMLIB_WASAPI
 * \param[in]	type		サウンドレンダラタイプ
 * \return		CriBool		機能が有効かどうか（CRI_TRUE = 有効、CRI_FALSE = 無効）
 * \par 説明:
 * スペーシャルオーディオ機能が有効になっているかどうかをチェックします。<br>
 * 引数の type には、スペーシャルオーディオ機能が有効化どうかをチェックしたいサウンドレンダラを指定します。<br>
 */
CriBool CRIAPI criAtom_IsSpatialAudioEnabled_WASAPI(CriAtomSoundRendererType type);

#if defined(XPT_TGT_PC)
/*JP
 * \brief ハプティクスデバイスIDの指定
 * \ingroup ATOMLIB_WASAPI
 * \par 説明:
 * ハプティクスデバイスIDとそのタイプを指定します。<br>
 * この関数を実行した後、CRIATOM_SOUND_RENDERER_HAPTIC を使用してハプティクス出力用のASRラックを作成できます。
 */
void CRIAPI criAtom_SetHapticDeviceId_WASAPI(LPCWSTR device_id, CriAtomHapticDeviceType_WASAPI device_type);
#endif

/*==========================================================================
 *      CRI AtomEx API
 *=========================================================================*/
/*JP
 * \brief ライブラリ初期化用ワーク領域サイズの計算
 * \ingroup ATOMLIB_WASAPI
 * \param[in]	config		初期化用コンフィグ構造体
 * \return		CriSint32	ワーク領域サイズ
 * \par 説明:
 * ライブラリを使用するために必要な、ワーク領域のサイズを取得します。<br>
 * \par 備考:
 * ライブラリが必要とするワーク領域のサイズは、ライブラリ初期化用コンフィグ
 * 構造体（ ::CriAtomExConfig_WASAPI ）の内容によって変化します。<br>
 * <br>
 * 引数 config の情報は、関数内でのみ参照されます。<br>
 * 関数を抜けた後は参照されませんので、関数実行後に config の領域を解放しても
 * 問題ありません。
 * \attention
 * ::CriAtomExConfig_WASAPI 構造体のacf_infoメンバに値を設定している場合、本関数は失敗し-1を返します。<br>
 * 初期化処理内でACFデータの登録を行う場合は、本関数値を使用したメモリ確保ではなくADXシステムによる
 * メモリアロケーターを使用したメモリ確保処理が必要になります。
 * \sa CriAtomExConfig_WASAPI, criAtomEx_Initialize_WASAPI
 */
CriSint32 CRIAPI criAtomEx_CalculateWorkSize_WASAPI(const CriAtomExConfig_WASAPI *config);

/*JP
 * \brief ライブラリの初期化
 * \ingroup ATOMLIB_WASAPI
 * \param[in]	config		初期化用コンフィグ構造体
 * \param[in]	work		ワーク領域
 * \param[in]	work_size	ワーク領域サイズ
 * \par 説明:
 * ライブラリを初期化します。<br>
 * ライブラリの機能を利用するには、必ずこの関数を実行する必要があります。<br>
 * （ライブラリの機能は、本関数を実行後、 ::criAtomEx_Finalize_WASAPI 関数を実行するまでの間、
 * 利用可能です。）<br>
 * <br>
 * ライブラリを初期化する際には、ライブラリが内部で利用するためのメモリ領域（ワーク領域）
 * を確保する必要があります。<br>
 * ライブラリが必要とするワーク領域のサイズは、初期化用コンフィグ構造体の内容に応じて
 * 変化します。<br>
 * ワーク領域サイズの計算には、 ::criAtomEx_CalculateWorkSize_WASAPI 
 * 関数を使用してください。<br>
 * \par 備考:
 * ::criAtomEx_SetUserAllocator マクロを使用してアロケーターを登録済みの場合、
 * 本関数にワーク領域を指定する必要はありません。<br>
 * （ work に NULL 、 work_size に 0 を指定することで、登録済みのアロケーター
 * から必要なワーク領域サイズ分のメモリが動的に確保されます。）
 * <br>
 * 引数 config の情報は、関数内でのみ参照されます。<br>
 * 関数を抜けた後は参照されませんので、関数実行後に config の領域を解放しても
 * 問題ありません。
 * \attention
 * 本関数は内部的に以下の関数を実行します。<br>
 * 	- ::criAtomEx_Initialize
 * 	- ::criAtomExAsr_Initialize
 * 	- ::criAtomExHcaMx_Initialize
 * 本関数を実行する場合、上記関数を実行しないでください。<br>
 * <br>
 * 本関数を実行後、必ず対になる ::criAtomEx_Finalize_WASAPI 関数を実行してください。<br>
 * また、 ::criAtomEx_Finalize_WASAPI 関数を実行するまでは、本関数を再度実行しないでください。<br>
 * \sa CriAtomExConfig_WASAPI, criAtomEx_Finalize_WASAPI,
 * criAtomEx_SetUserAllocator, criAtomEx_CalculateWorkSize_WASAPI
 */
void CRIAPI criAtomEx_Initialize_WASAPI(
	const CriAtomExConfig_WASAPI *config, void *work, CriSint32 work_size);

/*JP
 * \brief ライブラリの終了
 * \ingroup ATOMLIB_WASAPI
 * \par 説明:
 * ライブラリを終了します。<br>
 * \attention
 * 本関数は内部的に以下の関数を実行します。<br>
 * 	- ::criAtomEx_Finalize
 * 	- ::criAtomExAsr_Finalize
 * 	- ::criAtomExHcaMx_Finalize
 * 本関数を実行する場合、上記関数を実行しないでください。<br>
 * <br>
 * ::criAtomEx_Initialize_WASAPI 関数実行前に本関数を実行することはできません。<br>
 * \sa criAtomEx_Initialize_WASAPI
 */
void CRIAPI criAtomEx_Finalize_WASAPI(void);

#ifdef __cplusplus
}
#endif

/***************************************************************************
 *      旧バージョンとの互換用
 *      For compatibility with old version
 ***************************************************************************/
#define CRIATOM_SOUND_RENDERER_SPATIAL_AUDIO CRIATOM_SOUND_RENDERER_DEFAULT
#define criAtom_ControlComInitialization_WASAPI(sw)
#define criAtom_EnableSpatialAudio_WASAPI(type)
#define criAtom_DisableSpatialAudio_WASAPI()

#endif	/* CRI_INCL_CRI_ATOM_WASAPI_H */

/* --- end of file --- */
