# AI Security Vision System - Production Docker Image
# Multi-stage build for optimized production deployment

# Stage 1: Build Environment
FROM ubuntu:20.04 AS builder

# 设置环境变量
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai

# 安装构建依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    wget \
    curl \
    unzip \
    python3 \
    python3-pip \
    # OpenCV dependencies
    libopencv-dev \
    libopencv-contrib-dev \
    # SQLite
    libsqlite3-dev \
    # Network libraries
    libcurl4-openssl-dev \
    # JSON library
    nlohmann-json3-dev \
    # Threading
    libtbb-dev \
    # RKNN dependencies (for RK3588)
    libnuma-dev \
    && rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR /build

# 复制源代码
COPY . .

# 创建构建目录
RUN mkdir -p build

# 配置和编译项目
WORKDIR /build/build
RUN cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DENABLE_RKNN=ON \
    -DENABLE_OPTIMIZATION=ON \
    -DENABLE_SECURITY=ON

RUN make -j$(nproc)

# Stage 2: Frontend Build
FROM node:18-alpine AS frontend-builder

WORKDIR /app

# 复制前端源代码
COPY web-ui/package*.json ./
RUN npm ci --only=production

COPY web-ui/ .
RUN npm run build

# Stage 3: Production Runtime
FROM ubuntu:20.04 AS runtime

# 设置环境变量
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai
ENV AI_SECURITY_VISION_ENV=production

# 安装运行时依赖
RUN apt-get update && apt-get install -y \
    # 基础运行时库
    libopencv-core4.2 \
    libopencv-imgproc4.2 \
    libopencv-imgcodecs4.2 \
    libopencv-videoio4.2 \
    libopencv-dnn4.2 \
    # SQLite
    libsqlite3-0 \
    # 网络库
    libcurl4 \
    # 系统工具
    ca-certificates \
    tzdata \
    # 进程管理
    supervisor \
    # 健康检查工具
    curl \
    # 日志工具
    logrotate \
    && rm -rf /var/lib/apt/lists/*

# 创建应用用户
RUN groupadd -r aibox && useradd -r -g aibox -s /bin/false aibox

# 创建应用目录
RUN mkdir -p /opt/aibox/{bin,config,logs,data,models,web} \
    && chown -R aibox:aibox /opt/aibox

# 复制编译好的二进制文件
COPY --from=builder /build/build/AISecurityVision /opt/aibox/bin/
COPY --from=builder /build/models/ /opt/aibox/models/
COPY --from=builder /build/config/ /opt/aibox/config/

# 复制前端构建文件
COPY --from=frontend-builder /app/dist/ /opt/aibox/web/

# 复制配置文件
COPY docker/supervisor.conf /etc/supervisor/conf.d/aibox.conf
COPY docker/logrotate.conf /etc/logrotate.d/aibox
COPY docker/entrypoint.sh /opt/aibox/entrypoint.sh
COPY docker/healthcheck.sh /opt/aibox/healthcheck.sh

# 设置权限
RUN chmod +x /opt/aibox/entrypoint.sh /opt/aibox/healthcheck.sh \
    && chown -R aibox:aibox /opt/aibox

# 创建数据卷
VOLUME ["/opt/aibox/data", "/opt/aibox/logs", "/opt/aibox/config"]

# 暴露端口
EXPOSE 8080 8161 8162 8163 8164 3000

# 健康检查
HEALTHCHECK --interval=30s --timeout=10s --start-period=60s --retries=3 \
    CMD /opt/aibox/healthcheck.sh

# 设置工作目录
WORKDIR /opt/aibox

# 使用非root用户运行
USER aibox

# 入口点
ENTRYPOINT ["/opt/aibox/entrypoint.sh"]
CMD ["start"]
