# 🎤 학생 발표 평가 시스템

교사가 학생 명단을 업로드하고, 학생들이 QR코드로 접속하여 동료의 발표를 실시간으로 평가하는 웹앱입니다.

## 기능

### 교사
- 회원가입 / 로그인
- 반 생성 및 관리
- 학생 명단 CSV 업로드
- QR 코드 생성 (학생이 스마트폰으로 스캔)
- 발표 시작 / 종료 컨트롤
- **평가 결과 조회** (점수 + 코멘트, 교사만 접근 가능)

### 학생
- QR코드 스캔으로 접속
- 현재 발표자 자동 표시
- 1~10점 점수 선택
- 코멘트 입력 (선택)
- 자신의 발표는 평가 불가
- 1인 1평가 제한

## 기술 스택

- **백엔드**: Python 3.11, FastAPI, SQLAlchemy, SQLite
- **프론트엔드**: SvelteKit, Tailwind CSS
- **배포**: Docker + Docker Compose (Nginx)

---

## 실행 방법

### 방법 1: Docker Compose (전체)

```bash
cd presentation-scoring
docker-compose up --build
```

브라우저에서 `http://localhost` 접속

---

### 방법 2: 로컀 개발 (수동 실행)

**백엔드:**
```bash
cd presentation-scoring/backend
pip install -r requirements.txt
uvicorn main:app --reload --port 8000
```

**프론트엔드:**
```bash
cd presentation-scoring/frontend
npm install
npm run dev
```

브라우저에서 `http://localhost:5173` 접속

API 서류: `http://localhost:8000/docs`

---

## 사용 방법

### 1. 교사 로그인
`http://localhost` 접속 후 회원가입/로그인

### 2. 반 만들기
대시보드에서 **"새 반 만들기"** 클릭

### 3. 학생 명단 업로드

CSV 파일 형식:
```csv
이름,번호
김민준,1
이서연,2
박지훈,3
```

`sample_students.csv` 파일 수정 후 업로드

### 4. QR 코드 표시
- 반 관리 페이지에서 QR코드를 프로젝터/화면에 표시
- 학생들이 스마트폰 카메라로 QR코드 스캔

### 5. 발표 시작
- 발표자 선택 → **"🎤 발표 시작"** 클릭
- 학생들 화면에 발표자가 자동으로 나타남

### 6. 학생 평가
1. QR 스캔 → 화면 접속
2. 내 이름 선택
3. 1~10점 클릭
4. 코멘트 입력 (선택)
5. **"평가 제출하기"** 클릭

### 7. 결과 확인 (교사전용)
- **"평가 결과 보기"** 클릭
- 전체 점수, 평균, 모든 코멘트 확인

---

## API 문서

서버 실행 후 `http://localhost:8000/docs` 에서 Swagger UI 확인

## 환경 변수

| 변수 | 기본값 | 설명 |
|------|--------|------|
| `SECRET_KEY` | `dev-secret-key...` | JWT 시크릿 키 |
| `DATABASE_URL` | `sqlite:///./scoring.db` | DB 연결 문자열 |
