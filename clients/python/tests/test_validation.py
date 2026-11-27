import pytest

from easygamechat.easygamechat import (
    MAX_BUFFER_SIZE,
    MAX_MESSAGE_LENGTH,
    MAX_NICKNAME_LENGTH,
    is_secure_json,
    is_valid_message,
    is_valid_nickname,
)


@pytest.mark.parametrize(
    "nickname",
    [
        "alice",
        "Alice-1",
        "a_b",
        "a" * MAX_NICKNAME_LENGTH,
        "server1",
    ],
)
def test_valid_nicknames(nickname):
    assert is_valid_nickname(nickname)


@pytest.mark.parametrize(
    "nickname",
    [
        "",
        "a" * (MAX_NICKNAME_LENGTH + 1),
        "-alice",
        "alice!",
        "a__b",
        "a-_b",
        "admin",
        "SERVER",
        "al ice",
    ],
)
def test_invalid_nicknames(nickname):
    assert not is_valid_nickname(nickname)


@pytest.mark.parametrize(
    "message",
    [
        "hello",
        "hello world",
        "a" * MAX_MESSAGE_LENGTH,
        "{}[]~",
    ],
)
def test_valid_messages(message):
    assert is_valid_message(message)


@pytest.mark.parametrize(
    "message",
    [
        "",
        "   ",
        "a" * (MAX_MESSAGE_LENGTH + 1),
        "hello\nworld",
        "hello\tworld",
        "café",
        "hello\x7f",
    ],
)
def test_invalid_messages(message):
    assert not is_valid_message(message)


@pytest.mark.parametrize(
    "value",
    [
        '{"from":"alice","text":"hello"}',
        '{"text":"}"}',
        r'{"text":"\"}"}',
    ],
)
def test_secure_json(value):
    assert is_secure_json(value)


@pytest.mark.parametrize(
    "value",
    [
        "",
        "not json",
        '{"text":"hello"',
        '{"text":"hello"}}',
        '{"text":"hello}',
        "{" + "a" * MAX_BUFFER_SIZE + "}",
    ],
)
def test_insecure_json(value):
    assert not is_secure_json(value)
