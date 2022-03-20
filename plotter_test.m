close all;
clear all;

load('colorblind_colormap.mat');
%colormap(colorblind); % Sett etter figuren e laga.
data = {};
data{1}  = load('data\agg_2_siz_128\cleaned.txt');
data{2}  = load('data\agg_2_siz_256\cleaned.txt');
data{3}  = load('data\agg_2_siz_512\cleaned.txt');
data{4}  = load('data\agg_2_siz_1024\cleaned.txt');
data{5}  = load('data\agg_4_siz_128\cleaned.txt');
data{6}  = load('data\agg_4_siz_256\cleaned.txt');
data{7}  = load('data\agg_4_siz_512\cleaned.txt');
data{8}  = load('data\agg_4_siz_1024\cleaned.txt');
data{9}  = load('data\agg_8_siz_128\cleaned.txt');
data{10} = load('data\agg_8_siz_256\cleaned.txt');
data{11} = load('data\agg_8_siz_512\cleaned.txt');
data{12} = load('data\agg_8_siz_1024\cleaned.txt');
data{13} = load('data\agg_16_siz_128\cleaned.txt');
data{14} = load('data\agg_16_siz_256\cleaned.txt');
data{15} = load('data\agg_16_siz_512\cleaned.txt');
data{16} = load('data\agg_16_siz_1024\cleaned.txt');
data{17} = load('data\example\cleaned.txt');

x = [2, 4, 8, 16];
y = [128, 256, 512, 1024];
z = zeros(4);

for i=1:4
    for j=1:4
        z(i,j) = data{1,i*j}(8,2);
    end
end
