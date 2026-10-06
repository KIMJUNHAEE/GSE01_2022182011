# 코드 스타일

직접 작성한 `SimpleGame`의 C++ 소스·헤더와 `Shaders` 파일은 줄바꿈을 충분히 사용하는 스타일로 작성한다. 자동 포맷 기준은 루트의 `.clang-format`이다.

- 들여쓰기는 공백 4칸을 사용한다.
- 함수, 클래스, 구조체, 조건문, 반복문의 여는 중괄호는 다음 줄에 둔다(Allman).
- 한 줄에는 하나의 명령문을 작성한다.
- 짧은 함수와 람다도 본문을 여러 줄로 작성한다.
- 한 줄짜리 `if`, `else`, `for`, `while`에도 중괄호를 사용한다.
- 함수 정의 사이와 `public:`, `private:` 전후에는 빈 줄을 둔다.
- 연속 빈 줄은 한 줄까지 사용하고, 한 줄 길이는 100자를 기준으로 한다.
- 포인터와 참조는 타입에 붙인다: `Renderer*`, `const Prop&`.
- include 순서와 주석의 줄바꿈은 유지한다.

```cpp
void GameWorld::Advance()
{
    if (view != View::Dialogue)
    {
        return;
    }

    if (!dialogue.empty())
    {
        dialogue.pop_front();
    }
}
```

clang-format 22.1.3으로 적용 및 검증했다. 에디터에서 프로젝트의 `.clang-format`을 사용하도록 설정하면 같은 형식을 유지할 수 있다. `InsertBraces`를 지원하는 clang-format이 필요하며, 버전 차이에 따른 재정렬을 피하려면 같은 버전을 사용한다.

자동 포맷 대상은 `SimpleGame` 바로 아래의 `.cpp`, `.h`와 `SimpleGame/Shaders`의 `.vs`, `.fs`다. `Dependencies`의 외부 라이브러리 원본은 포맷 대상에 포함하지 않는다.

명령행에서는 C++ 파일에 `clang-format --style=file -i <파일>`을 사용한다. GLSL 파일은 `clang-format --style=file --assume-filename=shader.cpp -i <파일>`로 처리한다. 변경 없이 검사하려면 `-i` 대신 `--dry-run --Werror`를 사용한다.
