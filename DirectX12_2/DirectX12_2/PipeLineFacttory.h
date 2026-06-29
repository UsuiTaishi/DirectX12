#pragma once

const D3D12_RASTERIZER_DESC rasterizerSetting = 
{
	D3D12_FILL_MODE_WIREFRAME,
	D3D12_CULL_MODE_FRONT,
	TRUE,//ポリゴンの表裏の定義の仕方。
	0,//おもにZファイティング防止に使う。値が大きいほど手前に来る
	10,//ピクセルの最大深度バイアス
	4.5,//「ポリゴンの坂が急であればあるほど、奥に引っ込める量を自動でデカくする」ための倍率
	TRUE,//最大描画、最小描画距離外のポリゴンを描画しない設定
	FALSE,//マルチサンプリングアンチエイリアシングをオンにするかオフにするか
	FALSE,//アンチエイリアシングのオンオフ
	1,//レンダーターゲットがマルチサンプリングアンチエイリアシングに対応してない場合にラスタライズだけでマルチサンプリングするための倍率
	D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
	/* 2にするかどうか
	1. 通常のラスター化（デフォルト）
ルール: ポリゴンが、ピクセルの「中心点」をまたいだ時だけ、そのピクセルを塗りつぶします。

結果: ピクセルの端っこをかすめただけ（中心を通っていない）のポリゴンは、画面上では無視されて描画されません。

2. 保守的な（Conservative）ラスター化
ルール: ポリゴンのほんの少しの端っこが、ピクセルの領域に「1ミリでもかすったら」、中心を通っていなくてもそのピクセルを強制的に塗りつぶします。

結果: ポリゴンの一周回りぶん、少し太めに（漏れなく）塗りつぶされることになります。
	*/
};

const D3D12_DEPTH_STENCIL_DESC depthTest =
{
	TRUE,//深度テストを行うか否か
	D3D12_DEPTH_WRITE_MASK_ALL,//深度ステンシル バッファーへの書き込みの切り替え
	D3D12_COMPARISON_FUNC_LESS,//既存のデータを上書きする場合の条件
	FALSE,//ステンシルテストを行うか否か
	D3D12_DEFAULT_STENCIL_READ_MASK,//「今からステンシルバッファに数値を書き込むけど、このビットだけ書き換えて、それ以外は元の数字をキープして！」
	D3D12_DEFAULT_STENCIL_WRITE_MASK,
	{ // FrontFace (表面)
		D3D12_STENCIL_OP_KEEP,          // StencilFailOp
		D3D12_STENCIL_OP_KEEP,          // StencilDepthFailOp
		D3D12_STENCIL_OP_KEEP,          // StencilPassOp
		D3D12_COMPARISON_FUNC_ALWAYS    // StencilFunc
	},
	{ // BackFace (裏面)
		D3D12_STENCIL_OP_KEEP,
		D3D12_STENCIL_OP_KEEP,
		D3D12_STENCIL_OP_KEEP,
		D3D12_COMPARISON_FUNC_ALWAYS
	}
//なんかある
//なんかある
	};
/*
メッシュ（四角い板）を配置する

ラスタライズされる

GPUが、その四角い板を画面のピクセル（マスの目）に分解します。この時点では、透明な部分も不透明な部分も、すべて同じように「ピクセル」として処理の列に並びます。

ピクセルシェーダーが走る

テクスチャを読み込んで初めて、GPUは「あ、このマスは葉っぱの『透明な隙間』の部分だな（A＝0）」と気付きます。

discard でポイされる

条件（A＝0）に一致したピクセルだけがゴミ箱にポイされます。

アルファテスト::深度テストして手前にあるやつを検出した際にその手前のメッシュに透明部分がある場合ここをオンにしないと透明部分がない場合に消える部分が消えてしまう。

*/