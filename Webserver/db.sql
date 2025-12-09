CREATE TABLE DataPoint (
    id INT AUTO_INCREMENT PRIMARY KEY,
    datetime DATETIME NOT NULL,
    fanVoltage DECIMAL(5, 2) NOT NULL,
    fanCurrent DOUBLE(5, 2) NOT NULL,
    fanPower DOUBLE(5, 2) NOT NULL,
    pelVoltage DECIMAL(5, 2) NOT NULL,
    pelCurrent DOUBLE(5, 2) NOT NULL,
    pelPower DOUBLE(5, 2) NOT NULL,
    temperature DOUBLE(5, 2) NOT NULL,
    fan_status BOOLEAN DEFAULT FALSE,
    pel_status BOOLEAN DEFAULT FALSE
);