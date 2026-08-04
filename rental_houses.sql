-- ============================================================
-- 租房项目 - 房源表 houses 建表脚本(MySQL 8.0)
-- 在 Navicat 中:新建查询 → 粘贴本文件全部内容 → F6 运行
-- ============================================================

-- 1. 创建数据库(如果不存在)
CREATE DATABASE IF NOT EXISTS rental_db
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_unicode_ci;

-- 2. 切换到该数据库
USE rental_db;

-- 3. 删除旧表(方便反复测试,生产环境请删掉这一段)
DROP TABLE IF EXISTS houses;

-- 4. 创建房源表
CREATE TABLE houses (
    house_id        INT             NOT NULL AUTO_INCREMENT  COMMENT '房源ID',
    title           VARCHAR(100)    NOT NULL                 COMMENT '标题',
    description     VARCHAR(500)    DEFAULT NULL             COMMENT '描述',
    price           DECIMAL(10,2)   NOT NULL                 COMMENT '月租金(元)',
    area            DECIMAL(6,2)    NOT NULL                 COMMENT '面积(平方米)',
    layout          VARCHAR(20)     DEFAULT NULL             COMMENT '户型,如 2室1厅',
    floor           VARCHAR(20)     DEFAULT NULL             COMMENT '楼层,如 中层/5层',
    orientation     VARCHAR(20)     DEFAULT NULL             COMMENT '朝向,如 南北通透',
    city            VARCHAR(30)     DEFAULT NULL             COMMENT '城市',
    district        VARCHAR(30)     DEFAULT NULL             COMMENT '区域',
    address         VARCHAR(200)    DEFAULT NULL             COMMENT '详细地址',
    status          TINYINT         NOT NULL DEFAULT 0       COMMENT '状态:0可租 1已租 2下架',
    landlord_name   VARCHAR(30)     DEFAULT NULL             COMMENT '房东姓名',
    landlord_phone  VARCHAR(20)     DEFAULT NULL             COMMENT '房东电话',
    create_time     DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP                  COMMENT '创建时间',
    update_time     DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    PRIMARY KEY (house_id),
    KEY idx_city_status (city, status),
    KEY idx_price (price)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='房源信息表';

-- 5. 插入测试数据
INSERT INTO houses (title, description, price, area, layout, floor, orientation, city, district, address, status, landlord_name, landlord_phone) VALUES
    ('朝阳两居室 精装修 拎包入住', '靠近地铁,周边配套齐全,家电齐全', 5500.00, 78.50, '2室1厅', '中层/6层', '南北通透', '北京', '朝阳区', '建国路88号', 0, '张房东', '13800000001'),
    ('海淀三居室 学区房',         '紧邻名校,适合陪读家庭',          8800.00, 110.00, '3室2厅', '高层/18层', '南',     '北京', '海淀区', '中关村大街1号', 0, '李房东', '13800000002'),
    ('东直门开间 交通便利',       '地铁2号线直达,适合单身白领',      4200.00, 35.00,  '开间',   '低层/3层',  '东',     '北京', '东城区', '东直门内大街', 1, '王房东', '13800000003'),
    ('浦东两居室 江景房',         '俯瞰黄浦江,精装全配',            9500.00, 95.00,  '2室2厅', '高层/32层', '南',     '上海', '浦东新区', '陆家嘴环路100号', 0, '赵房东', '13800000004'),
    ('南山一居室 押一付一',       '近科技园,适合互联网从业者',      4800.00, 45.00,  '1室1厅', '中层/12层', '东南',   '深圳', '南山区', '科技园南区', 0, '钱房东', '13800000005');

-- 6. 验证查询
SELECT * FROM houses;
