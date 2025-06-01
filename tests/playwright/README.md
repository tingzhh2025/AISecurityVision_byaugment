# AI Security Vision API Consistency Tests

This directory contains comprehensive Playwright tests to verify the consistency between frontend and backend API endpoint handling in the AI Security Vision system.

## Overview

The test suite validates:
- **API Endpoint Consistency**: Ensures frontend API calls match backend endpoint implementations
- **Data Structure Validation**: Verifies response formats and data types are consistent
- **Error Handling**: Tests that both frontend and backend handle errors consistently
- **Request/Response Flow**: Validates the complete request-response cycle

## Test Files

### Core Test Files
- `api-consistency.spec.js` - Main API endpoint consistency tests
- `api-data-validation.spec.js` - Data structure and validation tests

### Configuration Files
- `playwright.config.js` - Playwright test configuration
- `package.json` - Node.js dependencies and scripts
- `global-setup.js` - Global test setup and initialization
- `global-teardown.js` - Global test cleanup

### Utility Files
- `run-tests.sh` - Automated test runner script
- `README.md` - This documentation file

## API Endpoints Tested

### System Endpoints
- `GET /api/system/status` - System status information
- `GET /api/system/info` - System information
- `GET /api/system/config` - System configuration
- `GET /api/system/metrics` - System performance metrics
- `GET /api/system/stats` - System statistics

### Camera Management
- `GET /api/cameras` - List all cameras
- `POST /api/cameras` - Add new camera
- `PUT /api/cameras/{id}` - Update camera
- `DELETE /api/cameras/{id}` - Delete camera
- `POST /api/cameras/test-connection` - Test camera connection

### Camera Configuration
- `GET /api/cameras/config` - Get camera configurations
- `POST /api/cameras/config` - Save camera configuration
- `DELETE /api/cameras/config/{id}` - Delete camera configuration

### Alert Management
- `GET /api/alerts` - Get alerts list
- `PUT /api/alerts/{id}/read` - Mark alert as read
- `DELETE /api/alerts/{id}` - Delete alert

### Detection Configuration
- `GET /api/detection/config` - Get detection configuration
- `PUT /api/detection/config` - Update detection configuration

### Person Statistics
- `GET /api/cameras/{id}/person-stats` - Get person statistics
- `POST /api/cameras/{id}/person-stats/enable` - Enable person statistics
- `POST /api/cameras/{id}/person-stats/disable` - Disable person statistics
- `GET /api/cameras/{id}/person-stats/config` - Get person stats configuration
- `POST /api/cameras/{id}/person-stats/config` - Update person stats configuration

## Prerequisites

1. **Backend Service**: The C++ backend must be built and available at `../../build/AISecurityVision`
2. **Frontend Service**: The Vue.js frontend must be available at `../../web-ui`
3. **Node.js**: Version 16 or higher
4. **System Dependencies**: The system should have the required dependencies for Playwright

## Installation

1. Navigate to the test directory:
```bash
cd tests/playwright
```

2. Install dependencies:
```bash
npm install
npx playwright install
npx playwright install-deps
```

## Running Tests

### Automated Test Runner (Recommended)

Use the provided shell script for automated testing:

```bash
# Run all tests with automatic service startup
./run-tests.sh

# Install Playwright dependencies only
./run-tests.sh --install

# Cleanup running services
./run-tests.sh --cleanup

# Show help
./run-tests.sh --help
```

### Manual Test Execution

1. Start the backend service:
```bash
cd ../../
./build/AISecurityVision --config config/system.json
```

2. Start the frontend service:
```bash
cd web-ui
npm run dev
```

3. Run the tests:
```bash
cd tests/playwright

# Run all tests
npm test

# Run specific test suites
npm run test:consistency
npm run test:validation

# Run with UI mode
npm run test:ui

# Run in headed mode (visible browser)
npm run test:headed

# Debug mode
npm run test:debug
```

## Test Configuration

### Environment Variables
- `BACKEND_PORT`: Backend service port (default: 8080)
- `FRONTEND_PORT`: Frontend service port (default: 3000)
- `CI`: Set to enable CI-specific configurations

### Playwright Configuration
The `playwright.config.js` file configures:
- Test timeout settings
- Browser configurations (Chrome, Firefox, Safari)
- Mobile device testing
- Report generation
- Service startup automation

## Test Reports

Tests generate multiple report formats:

### HTML Report
```bash
npx playwright show-report
```
- Interactive HTML report with test details
- Screenshots and videos for failed tests
- Located in `reports/html/`

### JSON Report
- Machine-readable test results
- Located in `reports/results.json`

### JUnit Report
- XML format for CI/CD integration
- Located in `reports/results.xml`

## Test Structure

### API Consistency Tests
1. **System Endpoints**: Validates system status, info, and configuration endpoints
2. **Camera Management**: Tests camera CRUD operations
3. **Alert Handling**: Verifies alert management functionality
4. **Detection Configuration**: Tests AI detection configuration
5. **Person Statistics**: Validates person statistics functionality
6. **Error Handling**: Tests error response consistency

### Data Validation Tests
1. **Camera Data Validation**: Validates camera object structure and field validation
2. **System Status Validation**: Tests system status response format
3. **Alert Data Validation**: Verifies alert object structure
4. **Person Statistics Validation**: Tests person stats data format
5. **Error Response Validation**: Validates error response formats

## Debugging

### Enable Debug Mode
```bash
npm run test:debug
```

### View Browser Actions
```bash
npm run test:headed
```

### Check Service Logs
- Backend logs: `backend.log`
- Frontend logs: `frontend.log`
- Frontend installation: `frontend-install.log`

### Common Issues

1. **Port Already in Use**: The script detects running services automatically
2. **Service Startup Timeout**: Increase timeout in configuration if needed
3. **Missing Dependencies**: Run `./run-tests.sh --install` to install all dependencies

## CI/CD Integration

The tests are designed for CI/CD integration:

```yaml
# Example GitHub Actions workflow
- name: Run API Consistency Tests
  run: |
    cd tests/playwright
    ./run-tests.sh
    
- name: Upload Test Results
  uses: actions/upload-artifact@v3
  with:
    name: playwright-report
    path: tests/playwright/reports/
```

## Contributing

When adding new API endpoints:

1. Add backend endpoint tests to `api-consistency.spec.js`
2. Add data validation tests to `api-data-validation.spec.js`
3. Update this README with the new endpoints
4. Ensure both positive and negative test cases are covered

## Troubleshooting

### Test Failures
1. Check service logs for errors
2. Verify API endpoints are responding correctly
3. Ensure frontend and backend are using consistent data formats

### Performance Issues
1. Increase timeouts in `playwright.config.js`
2. Reduce parallel test execution
3. Check system resources

### Network Issues
1. Verify ports are not blocked by firewall
2. Check if services are binding to correct interfaces
3. Ensure no proxy interference

## Support

For issues related to:
- **Test Framework**: Check Playwright documentation
- **API Endpoints**: Review backend API implementation
- **Frontend Integration**: Check Vue.js frontend code
- **System Integration**: Verify system configuration
