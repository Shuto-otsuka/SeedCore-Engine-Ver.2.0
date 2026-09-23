import os
import shutil

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))

# UserProject/Assets の下に常に在ってほしいフォルダ。
# 中身は消すがフォルダ自体は残し、無くなっていれば作り直す。
ASSET_FOLDERS = ['Audio', 'Config', 'Effect', 'Font', 'Image', 'Model', 'Movie', 'Prefab', 'Scene', 'Skymap']

# Reflection.py / Payload.py が「登録するものが何も無い」ときに書く内容。
# ファイル自体を消すと vcxproj の登録と食い違うので、中身だけを空に戻す。
GENERATED_FILES = [
    (os.path.join('FoundationEngine', 'Reflection', 'Reflection.generated.cpp'), 'FoundationEngine/Reflection/ReflectionRegistry.h', 'ScReflection'),
    (os.path.join('FoundationEngine', 'Payload', 'Payload.generated.cpp'), 'FoundationEngine/Payload/PayloadRegistry.h', 'ScPayload'),
    (os.path.join('UserProject', 'Reflection', 'Reflection.generated.cpp'), 'FoundationEngine/Reflection/ReflectionRegistry.h', 'ScReflection'),
    (os.path.join('UserProject', 'Payload', 'Payload.generated.cpp'), 'FoundationEngine/Payload/PayloadRegistry.h', 'ScPayload'),
]


def remove(path):
    """
    ファイルでもフォルダでも消す。開いている Visual Studio や Editor が
    掴んでいて消せないことがあるので、失敗しても止めずに知らせる。
    """
    try:
        if os.path.isdir(path) and not os.path.islink(path):
            shutil.rmtree(path)
        else:
            os.remove(path)
    except OSError as error:
        print(f'削除できません: {path} ({error.strerror})')
        return False
    print(f'削除: {path}')
    return True


def clean_folder_contents(relative_path):
    """
    フォルダの中身だけを消す。フォルダ自体は残す。
    """
    folder = os.path.join(PROJECT_ROOT, relative_path)
    if not os.path.exists(folder):
        return

    for entry in sorted(os.listdir(folder)):
        remove(os.path.join(folder, entry))


def clean_child_folders(relative_path):
    """
    フォルダ直下の各フォルダについて、その中身だけを消す。
    構成（Develop/Application のような区分）を保ったまま空にするため。
    """
    parent = os.path.join(PROJECT_ROOT, relative_path)
    if not os.path.exists(parent):
        return

    for entry in sorted(os.listdir(parent)):
        full = os.path.join(parent, entry)
        if os.path.isdir(full):
            for child in sorted(os.listdir(full)):
                remove(os.path.join(full, child))
        else:
            remove(full)


def reset_generated_file(relative_path, registry_include, namespace):
    path = os.path.join(PROJECT_ROOT, relative_path)
    if not os.path.exists(path):
        return

    lines = [
        '#include <FoundationEngine/Prelude.h>',
        f'#include <{registry_include}>',
        '',
        '',
        'namespace SeedCore',
        '{',
        f'\t namespace {namespace}',
        '\t {',
        '\t}',
        '}',
    ]
    code = '\r\n'.join(lines)

    with open(path, 'r', encoding='utf-8', newline='') as f:
        if f.read() == code:
            return

    with open(path, 'w', encoding='utf-8', newline='') as f:
        f.write(code)
    print(f'生成コードクリア: {relative_path}')


def main():
    # --- ユーザーが作ったもの ---
    clean_folder_contents(os.path.join('UserProject', 'Script'))
    clean_child_folders(os.path.join('UserProject', 'Assets'))

    # 決まった名前のフォルダは、消えていたら作り直す。
    # Editor 側がこの構成を前提に読み書きするため。
    for name in ASSET_FOLDERS:
        folder = os.path.join(PROJECT_ROOT, 'UserProject', 'Assets', name)
        if not os.path.exists(folder):
            os.makedirs(folder)
            print(f'フォルダ作成: {folder}')

    # --- コード生成が書き足したもの ---
    for relative_path, registry_include, namespace in GENERATED_FILES:
        reset_generated_file(relative_path, registry_include, namespace)

    # --- ビルドと実行で出たもの ---
    clean_child_folders('CompiledShaderObject')
    clean_folder_contents('Package')
    clean_folder_contents('Logs')

    # exe だけを消す。lib や pdb まで消すとリンクからやり直しになるため。
    runtime_build = os.path.join(PROJECT_ROOT, 'Runtime', 'Build')
    for root, _, files in os.walk(runtime_build):
        for name in files:
            if name.endswith('.exe'):
                remove(os.path.join(root, name))

    # Visual Studio の作業用フォルダ。開いたまま流すと掴まれていて消せない。
    solution_cache = os.path.join(PROJECT_ROOT, '.vs')
    if os.path.exists(solution_cache):
        remove(solution_cache)

    # --- 共有アセットの設定 ---
    sharing = os.path.join(PROJECT_ROOT, '.asset')
    if os.path.exists(sharing) and remove(sharing):
        print('共有アセットの設定を消しました。UserProject/Startup/Startup.bat をもう一度流してください')

    print('Clean.py 完了')


if __name__ == '__main__':
    main()
