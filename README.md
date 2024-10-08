# 500行以内で自作言語作ってみた

## 目次

1. [はじめに](#はじめに)
2. [特徴](#特徴)
3. [文法](#文法)
   3.1. [記号](#記号)
   3.2. [変数](#変数)
   3.3. [制御構文](#制御構文)
   3.4. [その他の機能](#その他の機能)
4. [実装詳細](#実装詳細)
5. [使用方法](#使用方法)
6. [制限事項](#制限事項)
7. [今後の展望](#今後の展望)
8. [感想](#感想)
9. [おわりに](#おわりに)

## 1. はじめに

こんにちは、みなさん！今回は「500行以内で自作言語作ってみた」というテーマで、私の苦労と喜びを詰め込んだ記事をお届けします。

まず、大事な注意事項からです：

- これはネタ記事です。真面目に受け取らないでください。
- 実用性？そんなの求めてませんよ。
- 私はプログラミング初心者です。温かい目で見守ってください。

さて、そんな前置きはさておき、私が作った言語の世界へ飛び込んでみましょう！

## 2. 特徴

我が愛すべき言語の特徴は以下の通りです：

1. **C言語で実装**: なぜC言語？それは...私がC言語しか知らなかったからです（笑）。
2. **動的メモリ確保なし**: はい、ご想像の通り、static配列の嵐です。メモリ管理？そんなの知りません。
3. **超シンプルな文法**: 「シンプルイズベスト」をモットーに、極限までシンプルにしました。使いやすいかどうかは... ノーコメントで。

## 3. 文法

### 3.1 記号

さあ、ここからが本領発揮です。なんと、この言語で使う記号はたったの4つ！

- '('
- ')'
- ','
- '.'

そう、括弧とカンマとピリオドだけです。なんなら'.'は必要ない時もあります。これで複雑なプログラムが書けるのか？いい質問ですね。答えは...書けません！

### 3.2 変数

変数と言っても、実質的には定数です。どういうことかというと：

```
local_get(a)
```

このように、変数にアクセスする度に関数っぽい呼び出しをします。スマートでしょう？いいえ、面倒くさいだけです。

### 3.3 制御構文

制御構文は、さすがに最低限は用意しました。

- if, else
- loop (breakとcontinueも使えます... たぶん)

使用例：

```
if(条件)
(
    処理
)
else
(
    別の処理
)

loop
(
    繰り返し処理
    if(条件) (break)
)
```

### 3.4 その他の機能

- 四則演算：add, sub, mul, div, modといった関数で実現
- デバッグ機能：ありません
- コメント機能：ありません（コードは十分に自己文書化されているはず... うん、きっと）

## 4. 実装詳細

実装は以下の5つのステップで行われます：

1. **トークン化** (`sxcscript_tokenize`): 
   ソースコードを意味のある最小単位（トークン）に分割します。

2. **構文解析** (`sxcscript_parse`): 
   トークンを解析して、言語の文法に従った中間表現を生成します。

3. **意味解析** (`sxcscript_analyze`): 
   変数の解決や命令の最適化を行います。

4. **リンク** (`sxcscript_link`): 
   ジャンプ命令のアドレス解決を行います。

5. **実行** (`sxcscript_exec`): 
   生成されたコードを実際に実行します。

各ステップの実装details... は、恥ずかしいのでここでは省略させていただきます。

## 5. 使用方法

使用方法は至って簡単です：

1. ソースコードを `test/02.txt` に保存します。
2. 以下のコマンドでコンパイルして実行します。

```bash
gcc -o sxcscript sxcscript.c
./sxcscript
```

簡単でしょう？ただし、プログラムが正しく動作する保証はありません。それはもう、神のみぞ知る世界です。

## 6. 制限事項

制限事項... というか、制限だらけですが、主なものを挙げると：

- エラー処理は最小限（というかほぼない）
- パフォーマンス？そんなの知らない
- 大規模なプログラムには向いていません（そもそも書けない）

## 7. 今後の展望

夢は大きく持とうということで、今後の展望を語らせていただきます：

- より多くの制御構文のサポート（for文とか、switch文とか）
- 関数のネストや再帰呼び出しの改善（今はバグの温床）
- デバッグ機能の追加（print文一つ追加するだけでも涙が出そう）

## 8. 感想

正直に申し上げますと、これは地獄の2週間でした。

- 圧倒的に技術力が足りない... 本当に何も分かっていませんでした。
- 特に関数定義が辛かった。スタックって何？ベースポインタって何？という感じでした。

そして、自分で自分の首を絞めるかのごとく、無茶な縛りを設けてしまいました：

- `fcntl.h`, `stdint.h`, `unistd.h` しかインクルードしていません。malloc不使用のオーガニックプログラミングです（白目）。
- そもそもC言語での実装が厳しかった... なぜPythonを選ばなかったのか。

## 9. おわりに

さて、ここまで読んでいただき、ありがとうございます。このプロジェクトは、間違いなく私のプログラミング人生において最も... 独特な経験となりました。

実用性はゼロに等しいかもしれませんが、このプロセスを通じて、プログラミング言語の基本的な概念や、実装の難しさについて多くを学ぶことができました。

最後に、この記事を読んで「俺ならもっと良いの作れる！」と思った方、ぜひチャレンジしてみてください。きっと、プログラミングの深淵を覗く素晴らしい経験になるはずです。

そして、このような無謀なプロジェクトにお付き合いいただいた皆様に、心からの感謝を。本当にありがとうございました！