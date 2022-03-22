close all;
clear all;

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

names = ["swim", "ammp", "galgel", "wupwise", "apsi", "applu", "art110", "art470", "bzip2\_graphic", "bzip2\_program", "bzip2\_source", "twolf"];

x = [2, 4, 8, 16];
y = [128, 256, 512, 1024];
speedups = zeros(4,4,12);
accuracies = zeros(4,4,12);
coverages = zeros(4,4,12);
for k=1:12
    for i=1:4
        for j=1:4
            speedups(i,j,k) = data{1,i*j}(k,2);
            accuracies(i,j,k) = data{1,i*j}(k,3);
            coverages(i,j,k) = data{1,i*j}(k,4);
        end
    end
end

speed = 1;
acc   = 2;
cov   = 3;
means = zeros(4,4,3);
for i=1:4   
    for j=1:4
        means(i,j,speed) = harmmean(speedups(i,j,:));
        means(i,j,acc)   = mean(accuracies(i,j,:));
        means(i,j,cov)   = mean(coverages(i,j,:));
    end
end

figure;
surf(x,y,means(:,:,speed));
xticks(x);
yticks(y);
xlabel("Prefetch degree");
ylabel("GHB Size");
zlabel("Speedup relative to no prefetching");

figure;
surf(x,y,means(:,:,acc));
xticks(x);
yticks(y);
xlabel("Prefetch degree");
ylabel("GHB Size");
zlabel("Prefetcher accuracy");

figure;
surf(x,y,means(:,:,cov));
xticks(x);
yticks(y);
xlabel("Prefetch degree");
ylabel("GHB Size");
zlabel("Coverage");

figure;
d = [reshape(speedups(1,:,:),[4,12]).', data{1,17}(:,2)];
b = bar(d, 'hist');
xticklabels(names);
yline(1);
axis([-inf inf 0.5 1.2]);
ylabel("Speedup relative to no prefetching");
legend('GHB size 128', 'GHB size 256', 'GHB size 512', 'GHB size 1024', 'Example prefetcher');
grid on;
