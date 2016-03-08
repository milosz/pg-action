CREATE FUNCTION users_function_trigger_notify() RETURNS trigger
    LANGUAGE plpgsql
    AS $$
DECLARE
BEGIN
  PERFORM pg_notify('user_added',CAST(NEW.id AS text)); -- include table.id as _text_
  RETURN NEW;
END;
$$;


CREATE TRIGGER users_trigger_notify_after_insert AFTER INSERT ON users FOR EACH ROW EXE
