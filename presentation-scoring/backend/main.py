from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse
from database import engine
import models
from routers import auth, classes, presentations, scores
import os

models.Base.metadata.create_all(bind=engine)

app = FastAPI(title="학생 발표 평가 시스템", version="1.0.0")

origins = os.getenv("ALLOWED_ORIGINS", "*").split(",")
app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(auth.router, prefix="/api/auth", tags=["인증"])
app.include_router(classes.router, prefix="/api/classes", tags=["반 관리"])
app.include_router(presentations.router, prefix="/api/presentations", tags=["발표"])
app.include_router(scores.router, prefix="/api/scores", tags=["평가"])


@app.get("/api/health")
def health_check():
    return {"status": "ok"}


# 프로덕션: SvelteKit 정적 빌드 서빙 (static/ 디렉토리 존재 시)
STATIC_DIR = os.path.join(os.path.dirname(__file__), "static")

if os.path.exists(STATIC_DIR):
    @app.get("/{full_path:path}")
    async def serve_spa(full_path: str):
        file_path = os.path.join(STATIC_DIR, full_path)
        if full_path and os.path.isfile(file_path):
            return FileResponse(file_path)
        return FileResponse(os.path.join(STATIC_DIR, "index.html"))
