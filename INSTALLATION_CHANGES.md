# Installation Changes Summary

## What Changed

The Radio Benziger server installation has been updated to use a proper system installation directory instead of depending on a user's home directory.

## Changes Made

### 1. Installation Directory
- **Before**: `/home/abhiram/projects/radiobenziger`
- **After**: `/opt/benziger_radio`

### 2. System Service Configuration
- **Service User**: Changed from `abhiram` to `root`
- **Working Directory**: Now uses `/opt/benziger_radio`
- **Python Environment**: Virtual environment created in `/opt/benziger_radio/venv`

### 3. Setup Process
The `setup_server.sh` script now:
1. Creates `/opt/benziger_radio` directory
2. Copies `radio_server.py` and `requirements.txt` to the installation directory
3. Creates a Python virtual environment in the installation directory
4. Installs dependencies in the system location
5. Sets proper ownership and permissions
6. Installs the systemd service with correct paths

### 4. Security Improvements
- Installation runs as `root` user (appropriate for system service)
- Proper file ownership and permissions
- Systemd security settings updated for new paths

### 5. New Scripts
- **`uninstall_server.sh`**: Complete removal of the installation
  - Stops and disables the service
  - Removes installation directory
  - Removes systemd service file
  - Option to re-enable nginx

## Benefits

1. **No Home Directory Dependency**: System can work regardless of user accounts
2. **Proper System Installation**: Follows Linux filesystem hierarchy standards
3. **Clean Uninstall**: Complete removal possible with uninstall script
4. **Better Security**: Proper ownership and permissions
5. **Production Ready**: Suitable for deployment on any Linux system

## Usage

### Install
```bash
./setup_server.sh
```

### Uninstall
```bash
./uninstall_server.sh
```

### Management
```bash
# View logs
sudo journalctl -u radio-benziger.service -f

# Restart service
sudo systemctl restart radio-benziger.service

# Check status
sudo systemctl status radio-benziger.service
```

## File Locations

- **Installation Directory**: `/opt/benziger_radio/`
- **Python Server**: `/opt/benziger_radio/radio_server.py`
- **Virtual Environment**: `/opt/benziger_radio/venv/`
- **Systemd Service**: `/etc/systemd/system/radio-benziger.service`
- **Dependencies**: `/opt/benziger_radio/requirements.txt` 