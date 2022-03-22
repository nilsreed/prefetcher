GHB_SIZES = [128, 256, 512, 1024];

MEM_REQS = 24.*GHB_SIZES + 8 + 2^12*8 + (2^12.*GHB_SIZES + 2^12)*4 + 2^12*8;

without_toolong_deltabuffers = 24.*GHB_SIZES + 8 + 2^12*8 + 2.*GHB_SIZES.*4 + 2^12*8;

disp(MEM_REQS);
disp(without_toolong_deltabuffers);
% plot(GHB_SIZES, MEM_REQS);