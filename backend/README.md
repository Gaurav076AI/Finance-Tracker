# Finance Tracker — C++ Backend

REST API server written in C++17. Serves the existing `frontend/` files and persists user data to disk.

**The frontend folder is not modified.** Open the app at `http://localhost:8080` after starting the server.

## Build & Run

```bash
cd backend
make
./build/finance_server
```

Or:

```bash
cd backend && make run
```

## Architecture

| File | Purpose |
|------|---------|
| `back.cpp` | Entry point, route handlers, wires store + HTTP server |
| `http_server.hpp` | POSIX socket HTTP/1.1 server + static file serving |
| `store.hpp` | In-memory database, aggregation, file persistence |
| `models.hpp` | User, Transaction, Dashboard structs |
| `json_util.hpp` | Lightweight JSON parse/build (no external libs) |

## Time Complexity

| Operation | Complexity | Notes |
|-----------|------------|-------|
| Register / login lookup | **O(1)** avg | `unordered_map` on email & session token |
| Add expense | **O(1)** amortized | `vector::push_back` |
| List expenses | **O(n)** | n = transaction count |
| Build dashboard | **O(n + k log k)** | One pass over n txs; sort k unique items |
| Route dispatch | **O(1)** avg | Hash map of method + path |

## API Endpoints

Base URL: `http://localhost:8080/api`

### Health
```bash
curl http://localhost:8080/api/health
```

### Register (starts with zero expenses)
```bash
curl -X POST http://localhost:8080/api/auth/register \
  -H "Content-Type: application/json" \
  -d '{
    "email": "gaurav@ku.edu.np",
    "password": "mypass",
    "isStudent": true,
    "inHostel": true,
    "budget": 15000
  }'
```

Non-student with loan:
```bash
curl -X POST http://localhost:8080/api/auth/register \
  -H "Content-Type: application/json" \
  -d '{
    "email": "staff@ku.edu.np",
    "password": "mypass",
    "isStudent": false,
    "maritalStatus": "married",
    "hasKids": true,
    "hasLoan": true,
    "loanAmount": 500000,
    "budget": 45000
  }'
```

### Login
```bash
curl -X POST http://localhost:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"email":"gaurav@ku.edu.np","password":"mypass"}'
```

Returns `token` — use in `Authorization: Bearer <token>` header.

### Dashboard
```bash
curl http://localhost:8080/api/dashboard \
  -H "Authorization: Bearer YOUR_TOKEN"
```

Response includes: `budget`, `spent`, `remaining`, `topItems`, `categories`, `recentTransactions`, loan info.

### Add expense
```bash
curl -X POST http://localhost:8080/api/expenses \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -d '{"name":"Momo","amount":140,"category":"food"}'
```

Categories: `food`, `transport`, `clothes`, `study`, `other`

### List expenses
```bash
curl http://localhost:8080/api/expenses \
  -H "Authorization: Bearer YOUR_TOKEN"
```

## Frontend Integration (when ready)

The frontend currently uses in-browser JavaScript. To connect it later **without changing UI files**, wire `fetch()` calls to:

| Frontend action | API call |
|-----------------|----------|
| Finish onboarding | `POST /api/auth/register` |
| Sign in | `POST /api/auth/login` |
| Load dashboard | `GET /api/dashboard` |
| Add expense | `POST /api/expenses` |

All responses include a `dashboard` object matching the stats the UI displays.

## Tests

```bash
cd backend
make test
```

## Data Storage

User data is saved to `backend/data/store.dat` after each write.
